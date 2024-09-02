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

	Material* mat0 = m_Application->GetMaterialManager()->GetMaterial(0);
	Material* mat1 = m_Application->GetMaterialManager()->GetMaterial(1);
	Material* mat2 = m_Application->GetMaterialManager()->GetMaterial(2);
	Material* mat3 = m_Application->GetMaterialManager()->GetMaterial(3);

	mat1->SetAlbedo({ 0.6f, 0.2f, 1.0f })->SetRoughness(0.1f)->SetMetalness(1.0f);
	mat2->SetAlbedo({ 0.0f, 0.1f, 0.3f })->SetRoughness(0.8f)->SetMetalness(0.0f);
	mat3->SetAlbedo({ 0.6f, 0.2f, 0.1f })->SetRoughness(0.5f)->SetMetalness(1.0f);

	{
		Transform cubeTransform{ { 0.0f, -0.05f, 0.0f } };
		cubeTransform.SetScale(XMFLOAT3{ 25.0f, 0.1f, 25.0f });

		CreateNewInstance(m_CubeGeometryHandle, cubeTransform)->SetMaterial(mat2);
	}

	CreateNewInstance(m_SphereGeometryHandle, { -4.0f, 1.0f, 4.0f })->SetMaterial(mat1);
	CreateNewInstance(m_SphereGeometryHandle, { 2.0f, 1.0f, -3.0f })->SetMaterial(mat2);
	{
		Transform cubeTransform{ { -0.5f, 1.0f, 0.0f } };
		cubeTransform.SetYaw(45.0f);

		CreateNewInstance(m_CubeGeometryHandle, cubeTransform)->SetMaterial(mat1);
	}

	{
		Transform cubeTransform{ { -1.5f, 6.0f, 0.0f } };
		cubeTransform.SetScale(XMFLOAT3{ 2.0f, 0.5f, 4.0f });

		CreateNewInstance(m_CubeGeometryHandle, cubeTransform)->SetMaterial(mat3);
	}
}

void VolumetricScene::OnUpdate(float deltaTime)
{
	
}


GeometryInstance* VolumetricScene::CreateNewInstance(size_t geometry, const Transform& transform)
{
	m_GeometryInstances.emplace_back(CreateGeometryInstance(geometry));
	m_GeometryInstances.back()->SetTransform(transform);

	return m_GeometryInstances.back();
}

