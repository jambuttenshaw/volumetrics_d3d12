#pragma once
#include "Core.h"

using Microsoft::WRL::ComPtr;


struct ShaderDesc
{
	const wchar_t* ShaderPath = nullptr;
	const wchar_t* EntryPoint = nullptr;
};


struct D3DGraphicsPipelineDesc
{
	UINT NumRootParameters = 0;
	D3D12_ROOT_PARAMETER1* RootParameters = nullptr;
	D3D12_ROOT_SIGNATURE_FLAGS RootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	std::vector<D3D12_INPUT_ELEMENT_DESC> InputLayout;

	ShaderDesc VertexShader;
	ShaderDesc PixelShader;

	std::vector<std::wstring> Defines;

	std::vector<DXGI_FORMAT> RenderTargetFormats;
	DXGI_FORMAT DSVFormat;

	D3D12_DEPTH_STENCIL_DESC DepthStencilDesc = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	D3D12_RASTERIZER_DESC RasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
};

struct D3DComputePipelineDesc
{
	// Root parameter descriptions
	UINT NumRootParameters = 0;
	D3D12_ROOT_PARAMETER1* RootParameters = nullptr;

	// Compute Shader
	const wchar_t* Shader = nullptr;
	const wchar_t* EntryPoint = nullptr;

	std::vector<D3D12_STATIC_SAMPLER_DESC> StaticSamplers;

	std::vector<std::wstring> Defines;
};



class D3DPipeline
{
public:
	D3DPipeline() = default;
	virtual ~D3DPipeline() = default;

	DISALLOW_COPY(D3DPipeline)
	DEFAULT_MOVE(D3DPipeline)

	virtual void Bind(ID3D12GraphicsCommandList* commandList) const = 0;
	

	inline ID3D12RootSignature* GetRootSignature() const { return m_RootSignature.Get(); }
	inline ID3D12PipelineState* GetPipelineState() const { return m_PipelineState.Get(); }

protected:
	ComPtr<ID3D12RootSignature> m_RootSignature;
	ComPtr<ID3D12PipelineState> m_PipelineState;
};

class D3DGraphicsPipeline : public D3DPipeline
{
public:
	D3DGraphicsPipeline() = default;
	D3DGraphicsPipeline(D3DGraphicsPipelineDesc* desc);

	void Create(D3DGraphicsPipelineDesc* desc);

	virtual void Bind(ID3D12GraphicsCommandList* commandList) const override
	{
		commandList->SetGraphicsRootSignature(m_RootSignature.Get());
		commandList->SetPipelineState(m_PipelineState.Get());
	}
};

class D3DComputePipeline : public D3DPipeline
{
public:
	D3DComputePipeline() = default;
	D3DComputePipeline(D3DComputePipelineDesc* desc);

	void Create(D3DComputePipelineDesc* desc);

	virtual void Bind(ID3D12GraphicsCommandList* commandList) const override
	{
		commandList->SetComputeRootSignature(m_RootSignature.Get());
		commandList->SetPipelineState(m_PipelineState.Get());
	}
};
