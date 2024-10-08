#include "pch.h"
#include "D3DPipeline.h"

#include "Core.h"
#include "D3DGraphicsContext.h"
#include "D3DShaderCompiler.h"
#include "Geometry/Geometry.h"


D3DGraphicsPipeline::D3DGraphicsPipeline(D3DGraphicsPipelineDesc* desc)
{
	Create(desc);
}

void D3DGraphicsPipeline::Create(D3DGraphicsPipelineDesc* desc)
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Create the root signature
	{
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		rootSignatureDesc.Init_1_1(desc->NumRootParameters, desc->RootParameters, 0, nullptr, desc->RootSignatureFlags);

		ComPtr<ID3DBlob> signature;
		THROW_IF_FAIL(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, nullptr));
	THROW_IF_FAIL(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
	}

	// Compile shaders
	ComPtr<IDxcBlob> vs, ps;
	THROW_IF_FAIL(D3DShaderCompiler::CompileFromFile(
		desc->VertexShader.ShaderPath,
		desc->VertexShader.EntryPoint,
		L"vs",
		desc->Defines,
		&vs));
	if (desc->PixelShader.ShaderPath)
	{
		THROW_IF_FAIL(D3DShaderCompiler::CompileFromFile(
			desc->PixelShader.ShaderPath,
			desc->PixelShader.EntryPoint,
			L"ps",
			desc->Defines,
			&ps));
	}

	// Create the pipeline description
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_RootSignature.Get();
		psoDesc.VS = D3DShaderCompiler::GetBytecodeFromBlob(vs.Get());
		psoDesc.PS = desc->PixelShader.ShaderPath ? D3DShaderCompiler::GetBytecodeFromBlob(ps.Get()) : D3DShaderCompiler::GetNullBytecode();

		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		psoDesc.SampleMask = UINT_MAX;
		psoDesc.RasterizerState = desc->RasterizerDesc;
		psoDesc.DepthStencilState = desc->DepthStencilDesc;

		psoDesc.InputLayout.NumElements = static_cast<UINT>(desc->InputLayout.size());
		psoDesc.InputLayout.pInputElementDescs = desc->InputLayout.data();

		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

		// NOTE: There might be 0 render targets in depth-only passes
		psoDesc.NumRenderTargets = static_cast<UINT>(desc->RenderTargetFormats.size());
		for (UINT i = 0; i < psoDesc.NumRenderTargets; i++)
		{
			psoDesc.RTVFormats[i] = desc->RenderTargetFormats.at(i);
		}

		psoDesc.DSVFormat = desc->DSVFormat;
		psoDesc.SampleDesc.Count = 1;
		psoDesc.SampleDesc.Quality = 0;

		THROW_IF_FAIL(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState)));
	}
}



D3DComputePipeline::D3DComputePipeline(D3DComputePipelineDesc* desc)
{
	Create(desc);
}

void D3DComputePipeline::Create(D3DComputePipelineDesc* desc)
{
	const auto device = g_D3DGraphicsContext->GetDevice();

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Create the compute root signature
	{
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
		const UINT numStaticSamplers = static_cast<UINT>(desc->StaticSamplers.size());
		rootSignatureDesc.Init_1_1(desc->NumRootParameters, desc->RootParameters, numStaticSamplers, numStaticSamplers > 0 ? desc->StaticSamplers.data() : nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

		ComPtr<ID3DBlob> signature;
		THROW_IF_FAIL(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, nullptr));
		THROW_IF_FAIL(device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
	}

	// Create the compute pipeline state
	{
		ComPtr<IDxcBlob> computeShader;
		THROW_IF_FAIL(D3DShaderCompiler::CompileFromFile(desc->Shader, desc->EntryPoint, L"cs", desc->Defines, &computeShader));

		D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = m_RootSignature.Get();
		psoDesc.CS.pShaderBytecode = computeShader->GetBufferPointer();
		psoDesc.CS.BytecodeLength = computeShader->GetBufferSize();
		THROW_IF_FAIL(device->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&m_PipelineState)));
	}
}
