#include "pch.h"
#include "VolumetricScene.h"

#include "Application/D3DApplication.h"

#include "Renderer/Geometry/Geometry.h"
#include "Renderer/Geometry/GeometryInstance.h"


VolumetricScene::VolumetricScene(D3DApplication* application, UINT maxGeometryInstances)
	: Scene(application, maxGeometryInstances)
{
	// Populate scene
	m_CubeGeometryHandle = AddGeometry(GeometryFactory::BuildUnitCube());
	m_SphereGeometryHandle = AddGeometry(GeometryFactory::BuildSphere(1.0f, 20, 20));

	{
		Transform cubeTransform{ { 0.0f, -0.05f, 0.0f } };
		cubeTransform.SetScale(XMFLOAT3{ 6.0f, 0.1f, 6.0f });

		CreateNewInstance(m_CubeGeometryHandle, cubeTransform);
	}

	CreateNewInstance(m_SphereGeometryHandle, { -4.0f, 1.0f, 4.0f });
	CreateNewInstance(m_SphereGeometryHandle, { 2.0f, 1.0f, -3.0f });
	{
		Transform cubeTransform{ { -0.5f, 1.0f, 0.0f } };
		cubeTransform.SetYaw(45.0f);

		CreateNewInstance(m_CubeGeometryHandle, cubeTransform);
	}

}

void VolumetricScene::OnUpdate(float deltaTime)
{
	
}


void VolumetricScene::CreateNewInstance(size_t geometry, const Transform& transform)
{
	m_GeometryInstances.emplace_back(CreateGeometryInstance(geometry));
	m_GeometryInstances.back()->SetTransform(transform);
}

