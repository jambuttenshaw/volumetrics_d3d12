#pragma once

using namespace DirectX;


class Camera
{
public:
	Camera()
	{
		m_ViewMatrix = XMMatrixIdentity();
		m_ProjectionMatrix = XMMatrixIdentity();
	}

	XMFLOAT3 GetPosition() const;
	
	inline float GetYaw() const { return m_Yaw; }
	inline float GetPitch() const { return m_Pitch; }

	inline void SetPosition(const XMVECTOR& position) { m_Position = position; m_ViewDirty = true; }
	inline void SetPosition(const XMFLOAT3& position) { m_Position = XMLoadFloat3(&position); m_ViewDirty = true; }

	void SetYaw(float yaw) { m_Yaw = yaw; ClampYaw(); m_ViewDirty = true; }
	void SetPitch(float pitch) { m_Pitch = pitch; ClampPitch(); m_ViewDirty = true; }

	inline void Translate(const XMVECTOR& translation) { m_Position += translation; m_ViewDirty = true; }
	inline void Translate(const XMFLOAT3& translation) { m_Position += XMLoadFloat3(&translation); m_ViewDirty = true; }

	void RotateYaw(float deltaYaw) { m_Yaw += deltaYaw; ClampYaw(); m_ViewDirty = true; }
	void RotatePitch(float deltaPitch) { m_Pitch += deltaPitch; ClampPitch(); m_ViewDirty = true; }

	inline const XMVECTOR& GetForward() { RebuildViewIfDirty(); return m_Forward; }
	inline const XMVECTOR& GetUp() { RebuildViewIfDirty(); return m_Up; }
	inline const XMVECTOR& GetRight() { RebuildViewIfDirty(); return m_Right; }
	inline const XMMATRIX& GetViewMatrix() { RebuildViewIfDirty(); return m_ViewMatrix; }

	// Projection
	inline void SetOrthographic(bool ortho) { m_Orthographic = ortho; m_ProjectionDirty = true; }
	inline void SetAspectRatio(float aspectRatio) { m_AspectRatio = aspectRatio; m_ProjectionDirty = true; }

	inline float GetNearPlane() const { return m_NearPlane; }
	inline float GetFarPlane() const { return m_FarPlane; }

	inline const XMMATRIX& GetProjectionMatrix() { RebuildProjIfDirty(); return m_ProjectionMatrix; }

private:
	void RebuildViewIfDirty();
	void RebuildProjIfDirty();

	void ClampYaw();
	void ClampPitch();

private:
	XMVECTOR m_Position{ 0.0f, 0.0f, 0.0f };

	float m_Yaw = 0.0f;
	float m_Pitch = 0.0f;

	XMMATRIX m_ViewMatrix;

	XMVECTOR m_Forward{ 0.0f, 0.0f, 1.0f };
	XMVECTOR m_Right{ 1.0f, 0.0f, 0.0f };
	XMVECTOR m_Up{ 0.0f, 1.0f, 0.0f };

	bool m_ViewDirty = true;	// Flags when view matrix requires reconstruction

	// Projection properties
	float m_NearPlane = 0.1f;
	float m_FarPlane = 100.0f;

	float m_FOV = 0.25f * XM_PI;
	float m_AspectRatio = 1.0f;

	bool m_Orthographic = false;
	float m_OrthographicWidth = 100.0f;
	float m_OrthographicHeight = 100.0f;


	XMMATRIX m_ProjectionMatrix;

	bool m_ProjectionDirty = true;
};
