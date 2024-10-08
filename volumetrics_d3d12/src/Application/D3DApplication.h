#pragma once

#include "BaseApplication.h"
#include "Scene.h"

#include "Renderer/D3DGraphicsContext.h"
#include "Renderer/Buffer/TextureLoader.h"
#include "Renderer/Lighting/LightManager.h"
#include "Renderer/Lighting/Material.h"

#include "Framework/GameTimer.h"
#include "Framework/Camera/Camera.h"
#include "Framework/Camera/OrbitalCameraController.h"
#include "Renderer/DeferredRenderer.h"


// Forward declarations
class RenderItem;

using Microsoft::WRL::ComPtr;
using namespace DirectX;


class D3DApplication : public BaseApplication
{
public:
	D3DApplication(UINT width, UINT height, const std::wstring& name);
	virtual ~D3DApplication() override = default;

	DISALLOW_COPY(D3DApplication)
	DISALLOW_MOVE(D3DApplication)

	virtual bool ParseCommandLineArgs(LPWSTR argv[], int argc) override;

	virtual void OnInit() override;
	virtual void OnUpdate() override;
	virtual void OnRender() override;
	virtual void OnDestroy() override;

	virtual void OnResized() override;

	virtual bool GetTearingSupport() const override;
	virtual IDXGISwapChain* GetSwapChain() const override;

	inline const InputManager* GetInputManager() const { return m_InputManager.get(); }
	inline CameraController* GetCameraController() const { return m_CameraController.get(); }
	inline LightManager* GetLightManager() const { return m_LightManager.get(); }
	inline MaterialManager* GetMaterialManager() const { return m_MaterialManager.get(); }

	inline bool GetPaused() const { return m_Paused; }
	inline void SetPaused(bool paused) { m_Paused = paused; }

private:

	void UpdatePassCB();

	void InitImGui();

	void BeginUpdate();
	void EndUpdate();

	// ImGui Windows
	bool ImGuiApplicationInfo();

private:
	std::unique_ptr<D3DGraphicsContext> m_GraphicsContext;
	std::unique_ptr<TextureLoader> m_TextureLoader;

	PassConstantBuffer m_PassCB;
	XMMATRIX m_PrevView;
	XMMATRIX m_PrevProj;

	GameTimer m_Timer;
	Camera m_Camera;

	bool m_UseOrbitalCamera = false;
	std::unique_ptr<CameraController> m_CameraController;

	std::unique_ptr<Scene> m_Scene;

	std::unique_ptr<DeferredRenderer> m_DeferredRenderer;

	std::unique_ptr<LightManager> m_LightManager;
	std::unique_ptr<MaterialManager> m_MaterialManager;

	// GUI
	inline static constexpr int m_MinFontSize = 10;
	inline static constexpr int m_MaxFontSize = 20;
	int m_FontSize = 18;
	std::map<int, struct ImFont*> m_Fonts;

	bool m_DisableGUI = false;
	bool m_ShowMainMenuBar = true;
	bool m_ShowApplicationInfo = true;
	bool m_ShowSceneInfo = true;
	bool m_ShowSceneControls = true;

	float m_TimeScale = 1.0f;
	bool m_Paused = false;

	// Should the application toggle fullscreen on the next update
	bool m_ToggleFullscreen = false;
	D3DGraphicsContextFlags m_GraphicsContextFlags;
};
