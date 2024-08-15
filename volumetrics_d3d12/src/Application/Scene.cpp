#include "pch.h"
#include "Scene.h"

#include "imgui.h"

#include "Framework/Math.h"

#include "D3DApplication.h"

#include "pix3.h"


Scene::Scene(D3DApplication* application, UINT maxGeometryInstances)
	: m_Application(application)
	, m_MaxGeometryInstances(maxGeometryInstances)
	, m_ObjectCBs(D3DGraphicsContext::GetBackBufferCount())
{
	ASSERT(m_Application, "Invalid app.");

	// As there is a fixed max of instances, reserving them in advance ensures the vector will never re-allocate
	m_GeometryInstances.reserve(m_MaxGeometryInstances);

	for (auto& cb : m_ObjectCBs)
	{
		cb.Allocate(g_D3DGraphicsContext->GetDevice(), maxGeometryInstances, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT, L"Object Constant Buffer");
	}
}


void Scene::PreRender()
{
	const auto& objectCB = m_ObjectCBs.at(g_D3DGraphicsContext->GetCurrentBackBuffer());

	for (auto& geometryInstance : m_GeometryInstances)
	{
		if (geometryInstance.IsDirty())
		{
			ObjectConstantBuffer cbData = {};
			cbData.WorldMat = XMMatrixTranspose(geometryInstance.GetTransform().GetWorldMatrix());
			const Material* material = geometryInstance.GetMaterial();
			cbData.MaterialID = material ? material->GetMaterialID() : 0;

			objectCB.CopyElement(geometryInstance.GetInstanceID(), cbData);

			geometryInstance.DecrementFramesDirty();
		}
	}
}


size_t Scene::AddGeometry(std::unique_ptr<TriangleGeometry>&& geometry)
{
	m_Geometries.emplace_back(std::move(geometry));
	return m_Geometries.size() - 1;
}


GeometryInstance* Scene::CreateGeometryInstance(size_t geometryHandle)
{
	if (m_GeometryInstances.size() == m_MaxGeometryInstances)
	{
		LOG_WARN("Maximum geometry instances reached!");
		return nullptr;
	}

	UINT instanceID = static_cast<UINT>(m_GeometryInstances.size());
	m_GeometryInstances.emplace_back(instanceID, m_Geometries.at(geometryHandle).get());
	return &m_GeometryInstances.back();
}


D3D12_GPU_VIRTUAL_ADDRESS Scene::GetObjectCBAddress(UINT object) const
{
	return m_ObjectCBs.at(g_D3DGraphicsContext->GetCurrentBackBuffer()).GetAddressOfElement(object);
}



bool Scene::DisplayGeneralGui() const
{
	bool open = false;
	if (ImGui::Begin("Scene", &open))
	{
	}
	ImGui::End();
	return open;
}
