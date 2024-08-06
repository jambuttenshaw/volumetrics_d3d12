#include "pch.h"
#include "Geometry.h"

#include "Renderer/D3DGraphicsContext.h"


std::unique_ptr<TriangleGeometry> GeometryFactory::BuildUnitCube()
{
	// Geometry definition:
	VertexType vertices[] = {
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

	IndexType indices[] = {
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
	VertexType vertices[] = {
		{ { -1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 1.0f, 0.0f, 1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { 1.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } },
		{ { -1.0f, 0.0f, -1.0f }, { 0.0f, 1.0f, 0.0f } }
	};
	IndexType indices[] = {
		0, 1, 2,
		0, 2, 3
	};

	return BuildFromVerticesIndices(ARRAYSIZE(vertices), vertices, ARRAYSIZE(indices), indices);
}


std::unique_ptr<TriangleGeometry> GeometryFactory::BuildSphere(float radius, UINT segments, UINT slices)
{
	std::vector<VertexType> vertices;
	vertices.resize((segments + 1) * slices + 2);

	constexpr float _pi = XM_PI;
	constexpr float _2pi = XM_2PI;

	vertices[0].Position = XMFLOAT3(0, radius, 0);
	for (UINT lat = 0; lat < slices; lat++)
	{
		const float a1 = _pi * static_cast<float>(lat + 1) / static_cast<float>(slices + 1);
		const float sin1 = sinf(a1);
		const float cos1 = cosf(a1);

		for (UINT lon = 0; lon <= segments; lon++)
		{
			const float a2 = _2pi * static_cast<float>(lon == segments ? 0 : lon) / static_cast<float>(segments);
			const float sin2 = sinf(a2);
			const float cos2 = cosf(a2);

			vertices[lon + lat * (segments + 1) + 1].Position = XMFLOAT3(sin1 * cos2 * radius, cos1 * radius, sin1 * sin2 * radius);
		}
	}
	vertices[vertices.size() - 1].Position = XMFLOAT3(0, -radius, 0);

	for (int n = 0; n < vertices.size(); n++)
	{
		XMStoreFloat3(&vertices[n].Normal, XMVector3Normalize(XMLoadFloat3(&vertices[n].Position)));
	}

	std::vector<IndexType> indices(vertices.size() * 6);

	UINT  i = 0;
	for (UINT lon = 0; lon < segments; lon++)
	{
		indices[i++] = static_cast<IndexType>(lon + 2);
		indices[i++] = static_cast<IndexType>(lon + 1);
		indices[i++] = static_cast<IndexType>(0);
	}

	//Middle
	for (UINT lat = 0; lat < slices - 1; lat++)
	{
		for (UINT lon = 0; lon < segments; lon++)
		{
			const UINT current = lon + lat * (segments + 1) + 1;
			const UINT next = current + segments + 1;

			indices[i++] = static_cast<IndexType>(current);
			indices[i++] = static_cast<IndexType>(current + 1);
			indices[i++] = static_cast<IndexType>(next + 1);

			indices[i++] = static_cast<IndexType>(current);
			indices[i++] = static_cast<IndexType>(next + 1);
			indices[i++] = static_cast<IndexType>(next);
		}
	}

	//Bottom Cap
	for (UINT lon = 0; lon < segments; lon++)
	{
		indices[i++] = static_cast<IndexType>(vertices.size() - 1);
		indices[i++] = static_cast<IndexType>(vertices.size() - (lon + 2) - 1);
		indices[i++] = static_cast<IndexType>(vertices.size() - (lon + 1) - 1);
	}

	return BuildFromVerticesIndices(vertices.size(), vertices.data(), indices.size(), indices.data());
}


std::unique_ptr<TriangleGeometry> GeometryFactory::BuildFromVerticesIndices(UINT vertexCount, const VertexType* vertices, UINT indexCount, const IndexType* indices)
{
	const auto device = g_D3DGraphicsContext->GetDevice();
	auto geometry = std::make_unique<TriangleGeometry>();

	// Set up vertex buffer
	geometry->VertexBuffer.Allocate(device, vertexCount, 0, L"VertexType Buffer");
	geometry->VertexBuffer.CopyElements(0, vertexCount, vertices);

	// Create vertex buffer view
	geometry->VertexBufferView.BufferLocation = geometry->VertexBuffer.GetAddressOfElement(0);
	geometry->VertexBufferView.SizeInBytes = sizeof(VertexType) * vertexCount;
	geometry->VertexBufferView.StrideInBytes = sizeof(VertexType);

	// Set up index buffer
	geometry->IndexBuffer.Allocate(device, indexCount, 0, L"IndexType Buffer");
	geometry->IndexBuffer.CopyElements(0, indexCount, indices);

	// Create index buffer view
	geometry->IndexBufferView.BufferLocation = geometry->IndexBuffer.GetAddressOfElement(0);
	geometry->IndexBufferView.SizeInBytes = sizeof(IndexType) * indexCount;
	geometry->IndexBufferView.Format = DXGI_FORMAT_R16_UINT;

	return geometry;
}
