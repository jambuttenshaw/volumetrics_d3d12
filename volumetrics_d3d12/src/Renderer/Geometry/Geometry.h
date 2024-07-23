#pragma once

#include "Core.h"
#include "Renderer/Buffer/UploadBuffer.h"

using namespace DirectX;


struct Vertex
{
	XMFLOAT3 Position;
	XMFLOAT3 Normal;

	inline static constexpr D3D12_INPUT_ELEMENT_DESC InputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
};

typedef UINT16 Index;


struct TriangleGeometry
{
	TriangleGeometry() = default;
	virtual ~TriangleGeometry() = default;

	DISALLOW_COPY(TriangleGeometry)
	DEFAULT_MOVE(TriangleGeometry)

	// TODO: Allow uploading of resources from upload heap to default
	UploadBuffer<Vertex> VertexBuffer;
	UploadBuffer<Index> IndexBuffer;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW IndexBufferView;
};


class GeometryFactory
{
public:
	GeometryFactory() = delete;

	static std::unique_ptr<TriangleGeometry> BuildUnitCube();
};
