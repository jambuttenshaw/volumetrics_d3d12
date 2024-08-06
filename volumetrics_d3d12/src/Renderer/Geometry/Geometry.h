#pragma once

#include "Core.h"
#include "Renderer/Buffer/UploadBuffer.h"

using namespace DirectX;


struct VertexType
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;

	inline static constexpr D3D12_INPUT_ELEMENT_DESC InputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
};

typedef UINT16 IndexType;


struct TriangleGeometry
{
	TriangleGeometry() = default;
	virtual ~TriangleGeometry() = default;

	DISALLOW_COPY(TriangleGeometry)
	DEFAULT_MOVE(TriangleGeometry)

	// TODO: Allow uploading of resources from upload heap to default
	UploadBuffer<VertexType> VertexBuffer;
	UploadBuffer<IndexType> IndexBuffer;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;
};


class GeometryFactory
{
public:
	GeometryFactory() = delete;

	static std::unique_ptr<TriangleGeometry> BuildUnitCube();
	static std::unique_ptr<TriangleGeometry> BuildPlane();
	static std::unique_ptr<TriangleGeometry> BuildSphere(float radius, UINT segments, UINT slices);

	static std::unique_ptr<TriangleGeometry> BuildFromVerticesIndices(UINT vertexCount, const VertexType* vertices, UINT indexCount, const IndexType* indices);

};
