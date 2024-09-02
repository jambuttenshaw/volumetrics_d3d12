#pragma once
#include "Renderer/Buffer/UploadBuffer.h"
#include "HlslCompat/StructureHlslCompat.h"

using namespace DirectX;


class Material
{
	friend class MaterialManager;

	Material(UINT materialID);
public:
	~Material() = default;

	DEFAULT_COPY(Material);
	DEFAULT_MOVE(Material);

	inline UINT GetMaterialID() const { return m_MaterialID; }
	inline const char* GetName() const { return m_Name.c_str(); }

	inline const XMFLOAT3& GetAlbedo() const { return m_Data.Albedo; }
	inline float GetRoughness() const { return m_Data.Roughness; }
	inline float GetMetalness() const { return m_Data.Metalness; }

	// returns pointers to itself to daisy chain calls
	inline Material* SetAlbedo(const XMFLOAT3& albedo) { m_Data.Albedo = albedo; SetDirty(); return this; }
	inline Material* SetRoughness(float roughness) { m_Data.Roughness = roughness; SetDirty(); return this; }
	inline Material* SetMetalness(float metalness) { m_Data.Metalness = metalness; SetDirty(); return this; }


	void DrawGui();

private:
	void SetDirty();

private:
	UINT m_MaterialID;
	std::string m_Name;

	MaterialGPUData m_Data;
	UINT m_NumFramesDirty;
};


class MaterialManager
{
public:
	MaterialManager(size_t capacity);
	~MaterialManager() = default;

	DISALLOW_COPY(MaterialManager)
	DISALLOW_MOVE(MaterialManager)

	void UploadMaterialData();
	D3D12_GPU_VIRTUAL_ADDRESS GetMaterialBufferAddress() const;

	Material* GetMaterial(size_t index) { return &m_Materials.at(index); }

	void DrawGui();
	bool DrawMaterialComboGui(const char* label, int& material);

private:
	std::vector<Material> m_Materials;
	std::vector<UploadBuffer<MaterialGPUData>> m_MaterialBuffers;
};
