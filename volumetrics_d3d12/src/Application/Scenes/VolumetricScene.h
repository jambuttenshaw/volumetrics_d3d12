#pragma once

#include "Application/Scene.h"


class VolumetricScene : public Scene
{
public:
	VolumetricScene(D3DApplication* application, UINT maxGeometryInstances);
	virtual ~VolumetricScene() = default;

	DISALLOW_COPY(VolumetricScene)
	DEFAULT_MOVE(VolumetricScene)

	virtual void OnUpdate(float deltaTime) override;

	virtual bool DisplayGui() override { return false; }

protected:

	void CreateNewInstance(size_t geometry, const Transform& transform);

protected:
	size_t m_CubeGeometryHandle = -1;
	size_t m_SphereGeometryHandle = -1;

	std::vector<GeometryInstance*> m_GeometryInstances;
};
