#pragma once

#include "Core.h"

#include "Framework/Transform.h"

#define INVALID_INSTANCE_ID ((1 << 24) - 1)

struct TriangleGeometry;


class GeometryInstance
{
public:
	GeometryInstance(UINT instanceID, TriangleGeometry* geometry);

	inline UINT GetInstanceID() const { return m_InstanceID; }
	inline TriangleGeometry* GetGeometry() const { return m_Geometry; }

	inline const Transform& GetTransform() const { return m_Transform; }
	void SetTransform(const Transform& transform);

	inline bool IsDirty() const { return m_FramesDirty > 0; }
	inline void DecrementFramesDirty() { --m_FramesDirty; }

private:
	UINT m_InstanceID = INVALID_INSTANCE_ID;
	TriangleGeometry* m_Geometry = nullptr;

	UINT m_FramesDirty;
	Transform m_Transform;
};
