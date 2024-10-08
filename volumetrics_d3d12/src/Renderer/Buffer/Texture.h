#pragma once
#include "Core.h"


using Microsoft::WRL::ComPtr;

using namespace DirectX;

class Texture
{
public:
	Texture() = default;
	Texture(const D3D12_RESOURCE_DESC* const desc, D3D12_RESOURCE_STATES initialState, const wchar_t* resourceName, const D3D12_CLEAR_VALUE* const clearValue = nullptr);
	virtual ~Texture() = default;

	DISALLOW_COPY(Texture);
	DEFAULT_MOVE(Texture);

	void Allocate(const D3D12_RESOURCE_DESC* const desc, D3D12_RESOURCE_STATES initialState, const wchar_t* resourceName, const D3D12_CLEAR_VALUE* const clearValue = nullptr);

	inline ID3D12Resource* GetResource() const { return m_Resource.Get(); }
	inline DXGI_FORMAT GetFormat() const { return m_Format; }

private:
	ComPtr<ID3D12Resource> m_Resource;
	DXGI_FORMAT m_Format = DXGI_FORMAT_UNKNOWN;
};
