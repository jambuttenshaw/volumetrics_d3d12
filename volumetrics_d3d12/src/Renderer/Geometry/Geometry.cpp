#include "pch.h"
#include "Geometry.h"

#include "Renderer/D3DGraphicsContext.h"


std::unique_ptr<TriangleGeometry> GeometryFactory::BuildUnitCube()
{
	const auto device = g_D3DGraphicsContext->GetDevice();
	auto cubeGeometry = std::make_unique<TriangleGeometry>();

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

	// Set up vertex buffer
	constexpr UINT64 vertexCount = ARRAYSIZE(vertices);
	cubeGeometry->VertexBuffer.Allocate(device, vertexCount, 0, L"Cube Vertex Buffer");
	cubeGeometry->VertexBuffer.CopyElements(0, vertexCount, vertices);

	// Create vertex buffer view
	cubeGeometry->VertexBufferView.BufferLocation = cubeGeometry->VertexBuffer.GetAddressOfElement(0);
	cubeGeometry->VertexBufferView.SizeInBytes = sizeof(Vertex) * vertexCount;
	cubeGeometry->VertexBufferView.StrideInBytes = sizeof(Vertex);
		
	// Set up index buffer
	constexpr UINT64 indexCount = ARRAYSIZE(indices);
	cubeGeometry->IndexBuffer.Allocate(device, indexCount, 0, L"Cube Index Buffer");
	cubeGeometry->IndexBuffer.CopyElements(0, indexCount, indices);

	// Create index buffer view
	cubeGeometry->IndexBufferView.BufferLocation = cubeGeometry->IndexBuffer.GetAddressOfElement(0);
	cubeGeometry->IndexBufferView.SizeInBytes = sizeof(Index) * indexCount;
	cubeGeometry->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;

	return cubeGeometry;
}
