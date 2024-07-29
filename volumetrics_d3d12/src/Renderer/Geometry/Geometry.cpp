#include "pch.h"
#include "Geometry.h"

#include "Renderer/D3DGraphicsContext.h"


std::unique_ptr<TriangleGeometry> GeometryFactory::BuildUnitCube()
{
	// Geometry definition:
	Vertex vertices[] = {
		// Front
		{ { -1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f } },
		{ { 1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f } },
		{ { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f } },
		{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f } },
		// Back
		{ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		{ { 1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } },
		// Left
		{ { -1.0f, 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
		{ { -1.0f, 1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f } },
		{ { -1.0f, -1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f } },
		{ { -1.0f, -1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f } },
		// Right
		{ { 1.0f, 1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } },
		{ { 1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } },
		// Top
		{ { -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
		// Bottom
		{ { -1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f } },
		{ { 1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f } },
		{ { 1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
		{ { -1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f } },
	};

	Index indices[] = {
		// Front
		0, 1, 2,
		0, 2, 3,
		// Back
		4, 5, 6,
		4, 6, 7,
		// Left
		8, 9, 10,
		8, 10, 11,
		// Right
		12, 13, 14,
		12, 14, 15,
		// Top
		16, 17, 18,
		16, 18, 19,
		// Bottom
		20, 21, 22,
		20, 22, 23
	};

	return BuildFromVerticesIndices(ARRAYSIZE(vertices), vertices, ARRAYSIZE(indices), indices);
}


std::unique_ptr<TriangleGeometry> GeometryFactory::BuildPlane()
{
	Vertex vertices[] = {
		{ { -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 1.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { -1.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } }
	};
	Index indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	return BuildFromVerticesIndices(ARRAYSIZE(vertices), vertices, ARRAYSIZE(indices), indices);
}


std::unique_ptr<TriangleGeometry> GeometryFactory::BuildFromVerticesIndices(UINT vertexCount, const Vertex* vertices, UINT indexCount, const Index* indices)
{
	const auto device = g_D3DGraphicsContext->GetDevice();
	auto geometry = std::make_unique<TriangleGeometry>();

	// Set up vertex buffer
	geometry->VertexBuffer.Allocate(device, vertexCount, 0, L"Vertex Buffer");
	geometry->VertexBuffer.CopyElements(0, vertexCount, vertices);

	// Create vertex buffer view
	geometry->VertexBufferView.BufferLocation = geometry->VertexBuffer.GetAddressOfElement(0);
	geometry->VertexBufferView.SizeInBytes = sizeof(Vertex) * vertexCount;
	geometry->VertexBufferView.StrideInBytes = sizeof(Vertex);

	// Set up index buffer
	geometry->IndexBuffer.Allocate(device, indexCount, 0, L"Index Buffer");
	geometry->IndexBuffer.CopyElements(0, indexCount, indices);

	// Create index buffer view
	geometry->IndexBufferView.BufferLocation = geometry->IndexBuffer.GetAddressOfElement(0);
	geometry->IndexBufferView.SizeInBytes = sizeof(Index) * indexCount;
	geometry->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;

	return geometry;
}
