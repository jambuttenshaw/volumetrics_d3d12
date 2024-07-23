#pragma once

#include "Core.h"

#include "Framework/Transform.h"
#include "HlslCompat/RaytracingHlslCompat.h"


class GeometryInstance
{
public:
	GeometryInstance(UINT instanceID, TriangleGeometry* geometry)
		: m_InstanceID(instanceID)
		, m_Geometry(geometry)
	{
		ASSERT(m_InstanceID != INVALID_INSTANCE_ID, "Invalid instance ID");
		ASSERT(m_Geometry, "Invalid Geometry");
	}

	inline UINT GetInstanceID() const { return m_InstanceID; }
	inline TriangleGeometry* Geometry() const { return m_Geometry; }

	inline const Transform& GetTransform() const { return m_Transform; }
	inline void SetTransform(const Transform& transform) { m_Transform = transform; m_IsDirty = true; }

	inline bool IsDirty() const { return m_IsDirty; }
	inline void ResetDirty() { m_IsDirty = false; }

private:
	UINT m_InstanceID = INVALID_INSTANCE_ID;
	TriangleGeometry* m_Geometry = nullptr;
	bool m_IsDirty = true;
	Transform m_Transform;
};
