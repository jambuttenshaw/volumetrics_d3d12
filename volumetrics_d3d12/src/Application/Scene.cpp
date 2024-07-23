#include "pch.h"
#include "Scene.h"

#include "imgui.h"

#include "Framework/Math.h"

#include "D3DApplication.h"

#include "pix3.h"


Scene::Scene(D3DApplication* application, UINT maxGeometryInstances)
	: m_Application(application)
	, m_MaxGeometryInstances(maxGeometryInstances)
{
	ASSERT(m_Application, "Invalid app.");

	{
		// As there is a fixed max of instances, reserving them in advance ensures the vector will never re-allocate
		m_GeometryInstances.reserve(m_MaxGeometryInstances);
	}
}


void Scene::PreRender()
{
	PIXBeginEvent(PIX_COLOR_INDEX(6), L"Scene Render");

	UpdateAccelerationStructure();

	PIXEndEvent();
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


void Scene::UpdateAccelerationStructure()
{
	return;
	/*
	// Update geometry
	m_AccelerationStructure->UpdateInstanceDescs(m_GeometryInstances);

	// Rebuild
	m_AccelerationStructure->Build();
	*/
}


bool Scene::DisplayGeneralGui() const
{
	bool open = true;
	if (ImGui::Begin("Scene", &open))
	{
	}
	ImGui::End();
	return open;
}
