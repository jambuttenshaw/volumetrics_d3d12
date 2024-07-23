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

	ShaderDesc VertexShader;
	ShaderDesc PixelShader;

	std::vector<std::wstring> Defines;
};

struct D3DComputePipelineDesc
{
	// Root parameter descriptions
	UINT NumRootParameters = 0;
	D3D12_ROOT_PARAMETER1* RootParameters = nullptr;

	// Compute Shader
	const wchar_t* Shader = nullptr;
	const wchar_t* EntryPoint = nullptr;

	std::vector<std::wstring> Defines;
};



class D3DPipeline
{
public:
	D3DPipeline() = default;
	virtual ~D3DPipeline() = default;

	DISALLOW_COPY(D3DPipeline)
	DEFAULT_MOVE(D3DPipeline)

	void Bind(ID3D12GraphicsCommandList* commandList) const
	{
		commandList->SetComputeRootSignature(m_RootSignature.Get());
		commandList->SetPipelineState(m_PipelineState.Get());
	}

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
};

class D3DComputePipeline : public D3DPipeline
{
public:
	D3DComputePipeline() = default;
	D3DComputePipeline(D3DComputePipelineDesc* desc);

	void Create(D3DComputePipelineDesc* desc);
};
