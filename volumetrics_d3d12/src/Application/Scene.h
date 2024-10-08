#pragma once


#include "HlslCompat/StructureHlslCompat.h"
#include "Renderer/Geometry/Geometry.h"
#include "Renderer/Geometry/GeometryInstance.h"


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

	// Passes ownership of the geometry to the scene
	// Returns a handle from which the geometry can be referenced
	size_t AddGeometry(std::unique_ptr<TriangleGeometry>&& geometry);
	GeometryInstance* CreateGeometryInstance(size_t geometryHandle);

	// Getters
	inline const std::vector<GeometryInstance>& GetAllGeometryInstances() const { return m_GeometryInstances; }
	inline const std::vector<std::unique_ptr<TriangleGeometry>>& GetAllGeometries() const { return m_Geometries; }

	D3D12_GPU_VIRTUAL_ADDRESS GetObjectCBAddress(UINT object = 0) const;

	// Gui
	virtual bool DisplayGui() { return false; }
	bool DisplayGeneralGui() const;

protected:
	D3DApplication* m_Application = nullptr;

private:
	// A description of all the different types of geometry in the scene
	std::vector<std::unique_ptr<TriangleGeometry>> m_Geometries;
	// A collection of all objects in the scene. Each object is an instance of some existing geometry
	UINT m_MaxGeometryInstances = 0;
	std::vector<GeometryInstance> m_GeometryInstances;

	// GPU Data for the geometry instances
	// One buffer for each set of frame resources
	std::vector<UploadBuffer<ObjectConstantBuffer>> m_ObjectCBs;
};
