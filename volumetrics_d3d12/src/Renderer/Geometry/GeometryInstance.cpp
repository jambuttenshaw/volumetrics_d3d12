#include "pch.h"
#include "GeometryInstance.h"

#include "Renderer/D3DGraphicsContext.h"


GeometryInstance::GeometryInstance(UINT instanceID, TriangleGeometry* geometry)
	: m_InstanceID(instanceID)
	, m_Geometry(geometry)
	, m_FramesDirty(D3DGraphicsContext::GetBackBufferCount())
{
	ASSERT(m_InstanceID != INVALID_INSTANCE_ID, "Invalid instance ID");
	ASSERT(m_Geometry, "Invalid Geometry");
}


void GeometryInstance::SetTransform(const Transform& transform)
{
	m_Transform = transform;
	m_FramesDirty = D3DGraphicsContext::GetBackBufferCount();
}
