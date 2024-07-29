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
	size_t m_CubeGeometryHandle = -1;
	size_t m_PlaneGeometryHandle = -1;

	GeometryInstance* m_CubeInstance = nullptr;
	GeometryInstance* m_PlaneInstance = nullptr;

};
