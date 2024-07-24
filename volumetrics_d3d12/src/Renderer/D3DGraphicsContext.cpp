#include "pch.h"
#include "D3DGraphicsContext.h"

#include "D3DDebugTools.h"
#include "Windows/Win32Application.h"

#include "D3DFrameResources.h"

#include "Framework/GameTimer.h"
#include "Framework/Camera/Camera.h"

#include "pix3.h"


D3DGraphicsContext* g_D3DGraphicsContext = nullptr;


D3DGraphicsContext::D3DGraphicsContext(HWND window, UINT width, UINT height, const D3DGraphicsContextFlags& flags)
	: m_WindowHandle(window)
	, m_ClientWidth(width)
	, m_ClientHeight(height)
	, m_Flags(flags)
	, m_BackBufferFormat(DXGI_FORMAT_R8G8B8A8_UNORM)
	, m_DepthStencilFormat(DXGI_FORMAT_D24_UNORM_S8_UINT)
{
	ASSERT(!g_D3DGraphicsContext, "Cannot initialize a second graphics context!");
	g_D3DGraphicsContext = this;

	LOG_INFO("Creating D3D12 Graphics Context");

	// Init PIX
	if (m_Flags.EnableGPUCaptures)
	{
		m_PIXCaptureModule = PIXLoadLatestWinPixGpuCapturerLibrary();
		if (m_PIXCaptureModule)
		{
			LOG_INFO("Succesfully loaded PIX GPU capture library.");
		}
		else
		{
			LOG_ERROR("Failed to load PIX GPU capture library.");
		}
	}

	// Initialize D3D components
	CreateAdapter();
 
	// Create m_Device resources
	CreateDevice();
	CreateCommandQueues();
	CreateSwapChain();
	CreateDescriptorHeaps();
	CreateCommandAllocator();
	CreateCommandList();

	CreateRTVs();
	CreateFrameResources();
	CreateDepthStencilBuffer();

	if (CheckRaytracingSupport())
	{
		// Create DXR resources
		CreateRaytracingInterfaces();
	}
	else
	{
		if (m_Flags.RequiresRaytracing)
		{
			ASSERT(false, "Application requries hardware raytracing, but the adapter does not support it.");
			throw std::exception();
		}
	}

	m_ImGuiResources = m_SRVHeap->Allocate(1);
	ASSERT(m_ImGuiResources.IsValid(), "Failed to alloc");

	CreateProjectionMatrix();

	m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(m_ClientWidth), static_cast<float>(m_ClientHeight));
	m_ScissorRect = CD3DX12_RECT(0, 0, static_cast<LONG>(m_ClientWidth), static_cast<LONG>(m_ClientHeight));

	// Close the command list and execute it to begin the initial GPU setup
	// The main loop expects the command list to be closed anyway
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	const auto fenceValue = m_DirectQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_CurrentFrameResources->SetFence(fenceValue);

	// Wait for any GPU work executed on startup to finish before continuing
	m_DirectQueue->WaitForIdleCPUBlocking();

	// Temporary resources that were created during initialization
	// can now safely be released that the GPU has finished its work
	ProcessAllDeferrals();

	LOG_INFO("D3D12 Graphics Context created succesfully.");
}

D3DGraphicsContext::~D3DGraphicsContext()
{
	// Ensure the GPU is no longer references resources that are about
	// to be cleaned up by the destructor
	WaitForGPUIdle();

	// Free allocations
	m_RTVs.Free();
	m_DSV.Free();
	m_ImGuiResources.Free();

	// Free frame resources
	for (UINT n = 0; n < s_FrameCount; n++)
		m_FrameResources[n].reset();

	// Free resources that themselves might free more resources
	ProcessAllDeferrals();

	if (m_InfoQueue)
	{
		(void)m_InfoQueue->UnregisterMessageCallback(m_MessageCallbackCookie);
	}

	if (m_PIXCaptureModule)
	{
		FreeLibrary(m_PIXCaptureModule);
	}
}


void D3DGraphicsContext::Present()
{
	PIXBeginEvent(PIX_COLOR_INDEX(4), L"Present");

	const auto flags = m_WindowedMode && !m_VSyncEnabled ? DXGI_PRESENT_ALLOW_TEARING : 0;
	const auto interval = m_VSyncEnabled ? 1 : 0;

	const HRESULT result = m_SwapChain->Present(interval, flags);
	if (FAILED(result))
	{
		switch(result)
		{
		case DXGI_ERROR_DEVICE_RESET:
		case DXGI_ERROR_DEVICE_REMOVED:
			LOG_FATAL("Present Failed: Device Removed!");
			break;
		default:
			LOG_FATAL("Present Failed: Unknown Error!");
			break;
		}
	}

	MoveToNextFrame();
	ProcessDeferrals(m_FrameIndex);

	PIXEndEvent();
}

bool D3DGraphicsContext::CheckDeviceRemovedStatus() const
{
	const HRESULT result = m_Device->GetDeviceRemovedReason();
	if (result != S_OK)
	{
		LOG_ERROR(L"Device removed reason: {}", DXException(result).ToString().c_str());
		return true;
	}
	return false;
}


void D3DGraphicsContext::StartDraw(const PassConstantBuffer& passCB) const
{
	m_CurrentFrameResources->CopyPassData(passCB);


	PIXBeginEvent(PIX_COLOR_INDEX(2), L"Start Draw");

	// Command list allocators can only be reset when the associated
	// command lists have finished execution on the GPU
	m_CurrentFrameResources->ResetAllocator();

	// Command lists can (and must) be reset after ExecuteCommandList() is called and before it is repopulated
	THROW_IF_FAIL(m_CommandList->Reset(m_CurrentFrameResources->GetCommandAllocator(), nullptr));

	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(2), "Begin Frame");

	// Indicate the back buffer will be used as a render target
	const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	m_CommandList->ResourceBarrier(1, &barrier);

	m_CommandList->RSSetViewports(1, &m_Viewport);
	m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

	// Setup descriptor heaps
	ID3D12DescriptorHeap* ppHeaps[] = { m_SRVHeap->GetHeap(), m_SamplerHeap->GetHeap() };
	m_CommandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	PIXEndEvent();
}

void D3DGraphicsContext::EndDraw() const
{
	PIXBeginEvent(PIX_COLOR_INDEX(3), L"End Draw");

	// Indicate that the back buffer will now be used to present
	const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_RenderTargets[m_FrameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	m_CommandList->ResourceBarrier(1, &barrier);

	PIXEndEvent(m_CommandList.Get());

	THROW_IF_FAIL(m_CommandList->Close());

	// Execute the command list
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	const auto fenceValue = m_DirectQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_CurrentFrameResources->SetFence(fenceValue);

	PIXEndEvent();
}

void D3DGraphicsContext::ClearBackBuffer(const XMFLOAT4& clearColor) const
{
	PIXBeginEvent(PIX_COLOR_INDEX(4), L"Clear Back Buffer");

	m_CommandList->ClearRenderTargetView(m_RTVs.GetCPUHandle(m_FrameIndex), &clearColor.x, 0, nullptr);
	m_CommandList->ClearDepthStencilView(m_DSV.GetCPUHandle(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	PIXEndEvent();
}

void D3DGraphicsContext::SetRenderTargetToBackBuffer(bool useDepth) const
{
	const D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_RTVs.GetCPUHandle(m_FrameIndex);
	const D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_DSV.GetCPUHandle();
	m_CommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, useDepth ? &dsvHandle : nullptr);
}

void D3DGraphicsContext::CopyToBackBuffer(ID3D12Resource* resource) const
{
	PIXBeginEvent(m_CommandList.Get(), PIX_COLOR_INDEX(71), "Copy to Back Buffer");

	const auto renderTarget = m_RenderTargets[m_FrameIndex].Get();

	D3D12_RESOURCE_BARRIER preCopyBarriers[2];
	preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_DEST);
	preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	m_CommandList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

	m_CommandList->CopyResource(renderTarget, resource);

	D3D12_RESOURCE_BARRIER postCopyBarriers[2];
	postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(renderTarget, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
	postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	m_CommandList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);

	PIXEndEvent(m_CommandList.Get());
}


D3D12_GPU_VIRTUAL_ADDRESS D3DGraphicsContext::GetPassCBAddress() const
{
	return m_CurrentFrameResources->GetPassCBAddress();
}

void D3DGraphicsContext::DeferRelease(const ComPtr<IUnknown>& resource) const
{
	m_CurrentFrameResources->DeferRelease(resource);
}



void D3DGraphicsContext::Resize(UINT width, UINT height)
{
	ASSERT(width && height, "Invalid client size!");

	m_ClientWidth = width;
	m_ClientHeight = height;

	// Wait for any work currently being performed by the GPU to finish
	WaitForGPUIdle();

	THROW_IF_FAIL(m_CommandList->Reset(m_DirectCommandAllocator.Get(), nullptr));

	// Release the previous resources that will be recreated
	for (UINT n = 0; n < s_FrameCount; ++n)
		m_RenderTargets[n].Reset();
	m_RTVs.Free();

	m_DepthBuffer.Reset();
	m_DSV.Free();

	// Process all deferred frees
	ProcessAllDeferrals();

	const auto flags = m_TearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	THROW_IF_FAIL(m_SwapChain->ResizeBuffers(s_FrameCount, m_ClientWidth, m_ClientHeight, m_BackBufferFormat, flags));

	BOOL fullscreenState;
	THROW_IF_FAIL(m_SwapChain->GetFullscreenState(&fullscreenState, nullptr));
	m_WindowedMode = !fullscreenState;

	m_FrameIndex = 0;
	CreateRTVs();
	CreateDepthStencilBuffer();

	// Send required work to re-init buffers
	THROW_IF_FAIL(m_CommandList->Close());
	ID3D12CommandList* ppCommandLists[] = { m_CommandList.Get() };
	const auto fenceValue = m_DirectQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_CurrentFrameResources->SetFence(fenceValue);

	// Recreate other objects while GPU is doing work
	CreateProjectionMatrix();

	// Wait for GPU to finish its work before continuing
	m_DirectQueue->WaitForIdleCPUBlocking();
}

void D3DGraphicsContext::WaitForGPUIdle() const
{
	m_DirectQueue->WaitForIdleCPUBlocking();
	m_ComputeQueue->WaitForIdleCPUBlocking();
}



void D3DGraphicsContext::CreateAdapter()
{
	UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
#ifdef ENABLE_INSTRUMENTATION
	LOG_WARN("Instrumentation is enabled - debug layer will be disabled.");
#else
	// Enable debug layer
	ComPtr<ID3D12Debug> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();

		dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

		LOG_INFO("D3D12 Debug Layer Created")
	}

	// Enable DRED
	ComPtr<ID3D12DeviceRemovedExtendedDataSettings1> dredSettings;
	if (m_Flags.EnableDRED && SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&dredSettings))))
	{
		// Turn on AutoBreadcrumbs and Page Fault reporting
		dredSettings->SetAutoBreadcrumbsEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
		dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);

		LOG_INFO("DRED Enabled")
	}
#endif
#endif

	THROW_IF_FAIL(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_Factory)));

	{
		BOOL allowTearing = FALSE;
		const HRESULT hr = m_Factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
		m_TearingSupport = SUCCEEDED(hr) && allowTearing;
	}

	for (UINT adapterIndex = 0;
		SUCCEEDED(m_Factory->EnumAdapterByGpuPreference(
			adapterIndex,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS(&m_Adapter)));
		++adapterIndex)
	{
		DXGI_ADAPTER_DESC1 desc;
		m_Adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		// Test to make sure this adapter supports D3D12, but don't create the actual m_Device yet
		if (SUCCEEDED(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{

#ifdef _DEBUG
			std::wstring deviceInfo;
			deviceInfo += L"Selected m_Device:\n\t";
			deviceInfo += desc.Description;
			deviceInfo += L"\n\tAvailable Dedicated Video Memory: ";
			deviceInfo += std::to_wstring(desc.DedicatedVideoMemory / 1000000000.f);
			deviceInfo += L" GB";
			LOG_INFO(deviceInfo.c_str());
#endif

			break;
		}
	}

	// make sure an adapter was created successfully
	ASSERT(m_Adapter, "Failed to find any adapter.");
}


void D3DGraphicsContext::CreateDevice()
{
	// Create the D3D12 Device object
	THROW_IF_FAIL(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_Device)));
	D3D_NAME(m_Device);

#ifdef _DEBUG
	// Set up the info queue for the device
	// Debug layer must be enabled for this: so only perform this in debug
	// Note that means that m_InfoQueue should always be checked for existence before use
	if (SUCCEEDED(m_Device->QueryInterface(IID_PPV_ARGS(&m_InfoQueue))))
	{
		// Set up message callback
		THROW_IF_FAIL(m_InfoQueue->RegisterMessageCallback(D3DDebugTools::D3DMessageHandler, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, &m_MessageCallbackCookie));
		THROW_IF_FAIL(m_InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true));
		LOG_INFO("D3D Info Queue message callback created.");
	}
	else
	{
		LOG_WARN("D3D Info Queue interface not available! D3D messages will not be received.");
	}
#endif
}

void D3DGraphicsContext::CreateCommandQueues()
{
	m_DirectQueue = std::make_unique<D3DQueue>(m_Device.Get(), D3D12_COMMAND_LIST_TYPE_DIRECT, L"Direct Queue");
	m_ComputeQueue = std::make_unique<D3DQueue>(m_Device.Get(), D3D12_COMMAND_LIST_TYPE_COMPUTE, L"Compute Queue");
}

void D3DGraphicsContext::CreateSwapChain()
{
	// Describe and create the swap chain
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = s_FrameCount;
	swapChainDesc.Width = m_ClientWidth;
	swapChainDesc.Height = m_ClientHeight;
	swapChainDesc.Format = m_BackBufferFormat;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Flags = m_TearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

	ComPtr<IDXGISwapChain1> swapChain;
	THROW_IF_FAIL(m_Factory->CreateSwapChainForHwnd(
		m_DirectQueue->GetCommandQueue(),			// Swap chain needs the queue so that it can force a flush on it.
		Win32Application::GetHwnd(),
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	// Currently no support for fullscreen transitions
	THROW_IF_FAIL(m_Factory->MakeWindowAssociation(Win32Application::GetHwnd(), DXGI_MWA_NO_ALT_ENTER));

	THROW_IF_FAIL(swapChain.As(&m_SwapChain));
	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
}

void D3DGraphicsContext::CreateDescriptorHeaps()
{
	m_RTVHeap = std::make_unique<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, s_FrameCount + 16, true);
	m_DSVHeap = std::make_unique<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16, true);

	// SRV/CBV/UAV heap
	constexpr UINT Count = 256;
	m_SRVHeap = std::make_unique<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, Count, false);

	m_SamplerHeap = std::make_unique<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 8, false);
}

void D3DGraphicsContext::CreateCommandAllocator()
{
	THROW_IF_FAIL(m_Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_DirectCommandAllocator)));
	D3D_NAME(m_DirectCommandAllocator);
}

void D3DGraphicsContext::CreateCommandList()
{
	// Create the command list
	THROW_IF_FAIL(m_Device->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		m_DirectCommandAllocator.Get(), nullptr,
		IID_PPV_ARGS(&m_CommandList)));
	D3D_NAME(m_CommandList);
}

void D3DGraphicsContext::CreateRTVs()
{
	m_RTVs = m_RTVHeap->Allocate(s_FrameCount);
	ASSERT(m_RTVs.IsValid(), "RTV descriptor alloc failed");

	// Create an RTV for each frame
	for (UINT n = 0; n < s_FrameCount; n++)
	{
		THROW_IF_FAIL(m_SwapChain->GetBuffer(n, IID_PPV_ARGS(&m_RenderTargets[n])));
		m_Device->CreateRenderTargetView(m_RenderTargets[n].Get(), nullptr, m_RTVs.GetCPUHandle(n));
		D3D_NAME(m_RenderTargets[n]);
	}
}

void D3DGraphicsContext::CreateFrameResources()
{
	m_FrameResources.clear();
	m_FrameResources.resize(s_FrameCount);
	for (auto& frameResources : m_FrameResources)
	{
		frameResources = std::make_unique<D3DFrameResources>();
	}
	m_CurrentFrameResources = m_FrameResources[m_FrameIndex].get();
}

void D3DGraphicsContext::CreateDepthStencilBuffer()
{
	// Create a resource for the depth buffer
	const auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const auto desc = CD3DX12_RESOURCE_DESC(
		D3D12_RESOURCE_DIMENSION_TEXTURE2D,
		0,
		m_ClientWidth,
		m_ClientHeight,
		1,
		1,
		m_DepthStencilFormat,
		1,
		0,
		D3D12_TEXTURE_LAYOUT_UNKNOWN,
		D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
	);
	const auto clearValue = CD3DX12_CLEAR_VALUE(m_DepthStencilFormat, 1.0f, 0);

	THROW_IF_FAIL(m_Device->CreateCommittedResource(
		&defaultHeap,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&m_DepthBuffer)));

	// Create DSV
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = m_DepthStencilFormat;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;

	m_DSV = m_DSVHeap->Allocate(1);
	ASSERT(m_DSV.IsValid(), "Failed to allocate DSV");
	m_Device->CreateDepthStencilView(m_DepthBuffer.Get(), &dsvDesc, m_DSV.GetCPUHandle());
}


bool D3DGraphicsContext::CheckRaytracingSupport() const
{
	ComPtr<ID3D12Device> testDevice;
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

	return SUCCEEDED(D3D12CreateDevice(m_Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
		&& SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
		&& featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
}

// Create raytracing m_Device and command list.
void D3DGraphicsContext::CreateRaytracingInterfaces()
{
	THROW_IF_FAIL(m_Device->QueryInterface(IID_PPV_ARGS(&m_DXRDevice)));
	D3D_NAME(m_DXRDevice);
	THROW_IF_FAIL(m_CommandList->QueryInterface(IID_PPV_ARGS(&m_DXRCommandList)));
	D3D_NAME(m_DXRCommandList);
}


void D3DGraphicsContext::CreateProjectionMatrix()
{
	m_ProjectionMatrix = XMMatrixPerspectiveFovLH(m_FOV, GetAspectRatio(), m_NearPlane, m_FarPlane);
}

void D3DGraphicsContext::MoveToNextFrame()
{
	PIXBeginEvent(PIX_COLOR_INDEX(5), L"Move to next frame");

	// Change the frame resources
	m_FrameIndex = m_SwapChain->GetCurrentBackBufferIndex();
	m_CurrentFrameResources = m_FrameResources[m_FrameIndex].get();

	// If they are still being processed by the GPU, then wait until they are ready
	m_DirectQueue->WaitForFenceCPUBlocking(m_CurrentFrameResources->GetFenceValue());

	PIXEndEvent();
}

void D3DGraphicsContext::ProcessDeferrals(UINT frameIndex) const
{
	if (m_FrameResources[frameIndex])
		m_FrameResources[frameIndex]->ProcessDeferrals();

	m_RTVHeap->ProcessDeferredFree(frameIndex);
	m_DSVHeap->ProcessDeferredFree(frameIndex);
	m_SRVHeap->ProcessDeferredFree(frameIndex);
	m_SamplerHeap->ProcessDeferredFree(frameIndex);
}

void D3DGraphicsContext::ProcessAllDeferrals() const
{
	for (UINT n = 0; n < s_FrameCount; ++n)
		ProcessDeferrals(n);
}
