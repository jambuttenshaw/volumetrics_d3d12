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

	m_CubeInstance = CreateGeometryInstance(m_CubeGeometryHandle);
}

void VolumetricScene::OnUpdate(float deltaTime)
{
	
}
