#pragma once


#include "Renderer/Geometry/Geometry.h"
#include "Renderer/Geometry/GeometryInstance.h"

#include "Renderer/Raytracing/AccelerationStructure.h"


class D3DApplication;


class Scene
{
public:
	Scene(D3DApplication* application, UINT maxGeometryInstances);
	virtual ~Scene() = default;

	DISALLOW_COPY(Scene)
	DEFAULT_MOVE(Scene)

	virtual void OnUpdate(float deltaTime) = 0;
	void PreRender();

	// TODO: How to describe geometry geometry.
	void AddGeometry();
	GeometryInstance* CreateGeometryInstance();

	// Getters
	inline const std::vector<TriangleGeometry*>& GetAllGeometries() const { return m_Geometries; }
	inline RaytracingAccelerationStructureManager* GetRaytracingAccelerationStructure() const { return m_AccelerationStructure.get(); }

	// Gui
	virtual bool DisplayGui() { return false; }
	bool DisplayGeneralGui() const;

private:
	// Handle changes to geometry and update acceleration structure
	void UpdateAccelerationStructure();

	// Debug Info
	void DisplayAccelerationStructureDebugInfo() const;

protected:
	D3DApplication* m_Application = nullptr;

private:
	// A description of all the different types of geometry in the scene
	std::vector<TriangleGeometry*> m_Geometries;
	// A collection of all objects in the scene. Each object is an instance of some existing geometry
	UINT m_MaxGeometryInstances = 0;
	std::vector<GeometryInstance> m_GeometryInstances;

	std::unique_ptr<RaytracingAccelerationStructureManager> m_AccelerationStructure;
};
