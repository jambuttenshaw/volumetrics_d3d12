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

	m_CubeInstance = CreateGeometryInstance(m_CubeGeometryHandle);
	m_SphereInstance = CreateGeometryInstance(m_SphereGeometryHandle);

	{
		Transform cubeTransform{ { 0.0f, -1.0f, 0.0f } };
		cubeTransform.SetScale(XMFLOAT3{ 6.0f, 0.1f, 6.0f });
		m_CubeInstance->SetTransform(cubeTransform);
	}

	{
		Transform sphereTransform{ { 0.0f, 1.0f, 2.0f } };
		m_SphereInstance->SetTransform(sphereTransform);
	}
}

void VolumetricScene::OnUpdate(float deltaTime)
{
	
}
