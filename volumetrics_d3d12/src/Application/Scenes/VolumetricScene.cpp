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
	m_PlaneGeometryHandle = AddGeometry(GeometryFactory::BuildPlane());

	m_CubeInstance = CreateGeometryInstance(m_CubeGeometryHandle);
	m_PlaneInstance = CreateGeometryInstance(m_PlaneGeometryHandle);

	Transform planeTransform{ { 0.0f, -1.0f, 0.0f } };
	planeTransform.SetScale(4.0f);
	m_PlaneInstance->SetTransform(planeTransform);

	const auto mat = m_Application->GetMaterialManager()->GetMaterial(1);
	mat->SetAlbedo({ 0.0f, 0.0f, 0.0f });
	mat->SetRoughness(1.0f);
		
	m_PlaneInstance->SetMaterial(mat);
}

void VolumetricScene::OnUpdate(float deltaTime)
{
	
}
