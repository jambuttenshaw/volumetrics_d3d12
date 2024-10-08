#pragma once

#include "Core.h"
#include "Memory/MemoryAllocator.h"

#include "D3DQueue.h"
#include "HlslCompat/StructureHlslCompat.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;

class RenderItem;
class GameTimer;
class Camera;

class Scene;

class D3DFrameResources;

// Make the graphics context globally accessible
class D3DGraphicsContext;
extern D3DGraphicsContext* g_D3DGraphicsContext;


struct D3DGraphicsContextFlags
{
	bool EnableGPUCaptures = false;
	bool EnableDRED = false;

	// Raytracing
	bool RequiresRaytracing = false; // Requires RT hardware support
};


class D3DGraphicsContext
{
public:
	D3DGraphicsContext(HWND window, UINT width, UINT height, const D3DGraphicsContextFlags& flags);
	~D3DGraphicsContext();

	// Disable copying & moving
	DISALLOW_COPY(D3DGraphicsContext)
	DISALLOW_MOVE(D3DGraphicsContext)


	void Present();
	// Returns true if device is removed
	bool CheckDeviceRemovedStatus() const;

	void StartDraw(const PassConstantBuffer& passCB) const;
	void EndDraw() const;

	void RestoreDefaultViewport() const;

	void ClearBackBuffer(const XMFLOAT4& clearColor) const;
	void SetRenderTargetToBackBuffer(bool useDepth) const;
	// For copying the output on a UAV into the back buffer
	void CopyToBackBuffer(ID3D12Resource* resource, D3D12_RESOURCE_STATES resourceState) const;

	void DeferRelease(const ComPtr<IUnknown>& resource) const;

	void Resize(UINT width, UINT height);

	// Wait for all queues to become idle
	void WaitForGPUIdle() const;

public:
	// Getters

	// Vsync
	inline bool GetVSyncEnabled() const { return m_VSyncEnabled; }
	inline void SetVSyncEnabled(bool enabled) { m_VSyncEnabled = enabled; }

	inline bool IsPIXEnabled() const { return m_PIXCaptureModule != nullptr; }

	inline float GetAspectRatio() const { return static_cast<float>(m_ClientWidth) / static_cast<float>(m_ClientHeight); }

	inline static constexpr UINT GetBackBufferCount() { return s_FrameCount; }
	inline DXGI_FORMAT GetBackBufferFormat() const { return m_BackBufferFormat; }

	inline UINT GetCurrentBackBuffer() const { return m_FrameIndex; }
	inline UINT64 GetTotalFrameCount() const { return m_TotalFrameCount; }

	inline UINT GetClientWidth() const { return m_ClientWidth; }
	inline UINT GetClientHeight() const { return m_ClientHeight; }

	// Raytracer needs access to the pass CB to render
	// Pass CB is accessed through the graphics context as that handles the buffering of the resource
	D3D12_GPU_VIRTUAL_ADDRESS GetPassCBAddress() const;

	// Descriptor heaps
	inline DescriptorHeap* GetSRVHeap() const { return m_SRVHeap.get(); }
	inline DescriptorHeap* GetSamplerHeap() const { return m_SamplerHeap.get(); }
	inline DescriptorHeap* GetRTVHeap() const { return m_RTVHeap.get(); }
	inline DescriptorHeap* GetDSVHeap() const { return m_DSVHeap.get(); }

	// For ImGui
	inline ID3D12DescriptorHeap* GetImGuiResourcesHeap() const { return m_ImGuiResources.GetHeap(); }
	inline D3D12_CPU_DESCRIPTOR_HANDLE GetImGuiCPUDescriptor() const { return m_ImGuiResources.GetCPUHandle(); }
	inline D3D12_GPU_DESCRIPTOR_HANDLE GetImGuiGPUDescriptor() const { return m_ImGuiResources.GetGPUHandle(); }

	// DXGI objects
	inline bool GetTearingSupport() const { return m_TearingSupport; }
	inline IDXGISwapChain* GetSwapChain() const { return m_SwapChain.Get(); }

	// D3D objects
	inline ID3D12Device* GetDevice() const { return m_Device.Get(); }
	inline ID3D12GraphicsCommandList* GetCommandList() const { return m_CommandList.Get(); }

	// Queues
	inline D3DQueue* GetDirectCommandQueue() const { return m_DirectQueue.get(); }
	inline D3DQueue* GetComputeCommandQueue() const { return m_ComputeQueue.get(); }

	// DXR objects
	inline ID3D12Device5* GetDXRDevice() const { return m_DXRDevice.Get(); }
	inline ID3D12GraphicsCommandList4* GetDXRCommandList() const { return m_DXRCommandList.Get(); }

private:
	// Startup
	void CreateAdapter();
	void CreateDevice();
	void CreateCommandQueues();
	void CreateSwapChain();
	void CreateDescriptorHeaps();
	void CreateCommandAllocator();
	void CreateCommandList();
	void CreateRTVs();
	void CreateFrameResources();
	void CreateDepthStencilBuffer();

	// Raytracing
	bool CheckRaytracingSupport() const;
	void CreateRaytracingInterfaces();

	void MoveToNextFrame();
	void ProcessDeferrals(UINT frameIndex) const;
	// NOTE: this is only safe to do so when ALL WORK HAS BEEN COMPLETED ON THE GPU!!!
	void ProcessAllDeferrals() const;

private:
	static constexpr UINT s_FrameCount = 2;
	UINT64 m_TotalFrameCount = 0;

	// Context properties
	HWND m_WindowHandle;
	UINT m_ClientWidth;
	UINT m_ClientHeight;
	D3DGraphicsContextFlags m_Flags;

	bool m_WindowedMode = true;
	bool m_TearingSupport = false;
	bool m_VSyncEnabled = false;

	CD3DX12_VIEWPORT m_Viewport{ };
	CD3DX12_RECT m_ScissorRect{ };

	// Formats
	DXGI_FORMAT m_BackBufferFormat;
	DXGI_FORMAT m_DepthStencilFormat;

	// Pipeline objects
	ComPtr<IDXGIAdapter1> m_Adapter;
	ComPtr<IDXGIFactory6> m_Factory;
	ComPtr<ID3D12Device> m_Device;

	// Device debug objects
	ComPtr<ID3D12InfoQueue1> m_InfoQueue;
	DWORD m_MessageCallbackCookie;

	HMODULE m_PIXCaptureModule = nullptr;

	ComPtr<IDXGISwapChain3> m_SwapChain;
	ComPtr<ID3D12Resource> m_RenderTargets[s_FrameCount];
	DescriptorAllocation m_RTVs;

	ComPtr<ID3D12Resource> m_DepthBuffer;
	DescriptorAllocation m_DSV;

	// Command queues
	std::unique_ptr<D3DQueue> m_DirectQueue;
	std::unique_ptr<D3DQueue> m_ComputeQueue;

	ComPtr<ID3D12CommandAllocator> m_DirectCommandAllocator;	// Used for non-frame-specific allocations (startup, resize swap chain, etc)
	ComPtr<ID3D12GraphicsCommandList> m_CommandList;

	// Frame resources
	UINT m_FrameIndex = 0;
	std::vector<std::unique_ptr<D3DFrameResources>> m_FrameResources;
	D3DFrameResources* m_CurrentFrameResources = nullptr;

	// Descriptor heaps
	std::unique_ptr<DescriptorHeap> m_RTVHeap;
	std::unique_ptr<DescriptorHeap> m_DSVHeap;
	std::unique_ptr<DescriptorHeap> m_SRVHeap;				// SRV, UAV, and CBV heap (named SRVHeap for brevity)
	std::unique_ptr<DescriptorHeap> m_SamplerHeap;

	// DirectX Raytracing (DXR) objects
	ComPtr<ID3D12Device5> m_DXRDevice;
	ComPtr<ID3D12GraphicsCommandList4> m_DXRCommandList;
	
	// ImGui Resources
	DescriptorAllocation m_ImGuiResources;
};
