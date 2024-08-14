#include "pch.h"
#include "LightManager.h"


#include "Renderer/D3DGraphicsContext.h"
#include "Framework/Math.h"

#include "IBL.h"

#include <pix3.h>
#include "imgui.h"


void CartesianDirectionToSpherical(const XMFLOAT3& direction, float& phi, float& theta)
{
	theta = acosf(direction.y);
	phi = Math::Sign(direction.z) * acosf(direction.x / sqrtf(direction.x * direction.x + direction.z * direction.z));
}


LightManager::LightManager()
{
	// Populate default light properties
	m_LightingCBStaging.DirectionalLight.Direction = { 0.0f, -0.7f, 0.7f };
	m_LightingCBStaging.DirectionalLight.Intensity = 2.0f;
	m_LightingCBStaging.DirectionalLight.Color = { 1.0f, 1.0f, 1.0f };

	m_LightingCBStaging.PointLightCount = s_MaxLights;

	for (auto& light : m_PointLightsStaging)
	{
		light.Position = { 0.0f, 2.0f, 0.0f };
		light.Color = { 1.0f, 1.0f, 1.0f };
		light.Intensity = 0.0f;
		light.Range = 3.0f;
	}

	// Set up shadow camera and shadow map
	// TODO: Sun shadow projection matrices will be handled a lot neater once cascaded shadow maps are implemented
	m_ShadowCameraProjectionMatrix = XMMatrixOrthographicLH(70.0f, 70.0f, 0.1f, 100.0f);

	constexpr UINT shadowMapW = 1024;
	constexpr UINT shadowMapH = 1024;
	m_SunShadowMap.CreateShadowMap(shadowMapW, shadowMapH);
	m_SunESM.CreateExponentialShadowMap(m_SunShadowMap);


	// Create light buffers
	m_LightingResources.resize(D3DGraphicsContext::GetBackBufferCount());
	for (auto& resources : m_LightingResources)
	{
		resources.LightingCB.Allocate(g_D3DGraphicsContext->GetDevice(), 1, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, L"Lighting Constant Buffer");
		resources.PointLights.Allocate(g_D3DGraphicsContext->GetDevice(), s_MaxLights, 0, L"Point Light Buffer");
	}

	// Create shadow sampler
	{
		m_ShadowSamplers = g_D3DGraphicsContext->GetSamplerHeap()->Allocate(ShadowSampler_Count);
		ASSERT(m_ShadowSamplers.IsValid(), "Failed to alloc!");

		D3D12_SAMPLER_DESC samplerDesc = {};
		samplerDesc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 0;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
		samplerDesc.BorderColor[0] = 1.0f;
		samplerDesc.BorderColor[1] = 1.0f;
		samplerDesc.BorderColor[2] = 1.0f;
		samplerDesc.BorderColor[3] = 1.0f;
		samplerDesc.MinLOD = 0.0f;
		samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

		g_D3DGraphicsContext->GetDevice()->CreateSampler(&samplerDesc, m_ShadowSamplers.GetCPUHandle(ShadowSampler_Comparison));

		samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

		g_D3DGraphicsContext->GetDevice()->CreateSampler(&samplerDesc, m_ShadowSamplers.GetCPUHandle(ShadowSampler_ESM));
	}

	m_IBL = std::make_unique<IBL>();
}

LightManager::~LightManager()
{
	m_ShadowSamplers.Free();
}

void LightManager::UpdateLightingCB(const XMFLOAT3& eyePos)
{
	// TODO: A nicer encapsulation of a shadow camera would be really nice
	// Create a view matrix for the directional light
	XMVECTOR forward = XMVector3Normalize(XMLoadFloat3(&m_LightingCBStaging.DirectionalLight.Direction));
	XMVECTOR up = { 0.0f, 1.0f, 0.0f };

	XMVECTOR right = XMVector3Normalize(XMVector3Cross(up, forward));
	up = XMVector3Cross(forward, right);

	// subtract a distance from the eye position to get a position for the camera
	constexpr float d = 50.0f;
	XMVECTOR position = - d * forward;

	XMMATRIX view = XMMatrixLookAtLH(position, position + forward, up);

	// Now update the CB with the new camera matrix
	const XMMATRIX viewProj = XMMatrixMultiply(view, m_ShadowCameraProjectionMatrix);
	m_LightingCBStaging.DirectionalLight.ViewProjection = XMMatrixTranspose(viewProj);
}

void LightManager::CopyStagingBuffers() const
{
	const auto& resources = m_LightingResources.at(g_D3DGraphicsContext->GetCurrentBackBuffer());

	resources.LightingCB.CopyElement(0, m_LightingCBStaging);
	resources.PointLights.CopyElements(0, static_cast<UINT>(m_PointLightsStaging.size()), m_PointLightsStaging.data());
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetLightingConstantBuffer() const
{
	return m_LightingResources.at(g_D3DGraphicsContext->GetCurrentBackBuffer()).LightingCB.GetAddressOfElement(0);
}

D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetPointLightBuffer() const
{
	return m_LightingResources.at(g_D3DGraphicsContext->GetCurrentBackBuffer()).PointLights.GetAddressOfElement(0);
}


void LightManager::DrawGui()
{

	// Directional Light
	{
		ImGui::Text("Directional Light");

		auto& directionalLight = m_LightingCBStaging.DirectionalLight;

		// Direction should be edited in spherical coordinates
		bool newDir = false;
		float phi, theta;
		CartesianDirectionToSpherical(directionalLight.Direction, phi, theta);

		newDir |= ImGui::SliderAngle("Theta", &theta, 1.0f, 179.0f);
		newDir |= ImGui::SliderAngle("Phi", &phi, -179.0f, 180.0f);
		if (newDir)
		{
			const float sinTheta = sinf(theta);
			const float cosTheta = cosf(theta);
			const float sinPhi = sinf(phi);
			const float cosPhi = cosf(phi);

			directionalLight.Direction.x = sinTheta * cosPhi;
			directionalLight.Direction.y = cosTheta;
			directionalLight.Direction.z = sinTheta * sinPhi;
		}

		ImGui::ColorEdit3("Sun Color", &directionalLight.Color.x);
		if (ImGui::DragFloat("Sun Intensity", &directionalLight.Intensity, 0.01f))
		{
			directionalLight.Intensity = max(0, directionalLight.Intensity);
		}
	}

	for (size_t i = 0; i < m_PointLightsStaging.size(); i++)
	{
		auto& light = m_PointLightsStaging.at(i);

		ImGui::Text("Light %d", i);

		ImGui::DragFloat3("Position", &light.Position.x, 0.01f);
		ImGui::ColorEdit3("Color", &light.Color.x);
		if (ImGui::DragFloat("Intensity", &light.Intensity, 0.01f))
		{
			light.Intensity = max(0.0f, light.Intensity);
		}
		if (ImGui::DragFloat("Range", &light.Range, 0.01f))
		{
			light.Range = max(0.0f, light.Range);
		}
	}

	static bool showShadowMap = true;
	if (ImGui::Begin("Shadow Map", &showShadowMap))
	{
		ImGui::Image(reinterpret_cast<ImTextureID>(m_SunShadowMap.GetSRV().ptr), { 400.0f, 400.0f });

		ImGui::End();
	}
}
