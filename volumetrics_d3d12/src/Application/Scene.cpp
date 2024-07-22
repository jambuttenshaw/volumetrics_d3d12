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
		// Set up acceleration structure
		constexpr D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		m_AccelerationStructure = std::make_unique<RaytracingAccelerationStructureManager>(m_MaxGeometryInstances);

		// Init top level AS
		m_AccelerationStructure->InitializeTopLevelAS(buildFlags, true, true, L"Top Level Acceleration Structure");

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


void Scene::AddGeometry()
{
	//m_Geometries.push_back(geometry);
}


GeometryInstance* Scene::CreateGeometryInstance()
{
	//m_GeometryInstances.emplace_back(instanceID);
	//return &m_GeometryInstances.back();
	return nullptr;
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
		DisplayAccelerationStructureDebugInfo();
	}
	ImGui::End();
	return open;
}

void Scene::DisplayAccelerationStructureDebugInfo() const
{
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
	ImGui::Text("Acceleration Structure");
	ImGui::PopStyleColor();

	auto DisplaySize = [](const char* label, UINT64 sizeKB)
	{
		if (false && sizeKB > 10'000)
			ImGui::Text("%s Size (MB): %d", label, sizeKB / 1024);
		else
			ImGui::Text("%s Size (KB): %d", label, sizeKB);
	};

	ImGui::Separator();
	{
		const auto& tlas = m_AccelerationStructure->GetTopLevelAccelerationStructure();
		DisplaySize("Top Level AS", tlas.GetResourcesSize() / 1024);
	}

	/*
	for (const auto& blasGeometry : m_Geometries)
	{
		const auto& blas = m_AccelerationStructure->GetBottomLevelAccelerationStructure(blasGeometry);
		DisplaySize("Bottom Level AS", blas.GetResourcesSize() / 1024);
	}
	*/
}
