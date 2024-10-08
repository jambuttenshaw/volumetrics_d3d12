#include "pch.h"
#include "D3DApplication.h"

#include "Windows/Win32Application.h"

#include "backends/imgui_impl_win32.h"
#include "backends/imgui_impl_dx12.h"
#include <imgui_internal.h>

#include "Renderer/Lighting/IBL.h"
#include "Renderer/Profiling/GPUProfiler.h"

#include <args.hxx>

#include "pix3.h"

#include <fstream>

#include "Renderer/D3DDebugTools.h"
#include "Framework/GuiHelpers.h"

#include "Scenes/VolumetricScene.h"


D3DApplication::D3DApplication(UINT width, UINT height, const std::wstring& name)
	: BaseApplication(width, height, name)
{
	m_Camera.SetAspectRatio(static_cast<float>(m_Width) / static_cast<float>(m_Height));
}


// convert wstring to UTF-8 string
#pragma warning( push )
#pragma warning( disable : 4244)
static std::string wstring_to_utf8(const std::wstring& str)
{
	return { str.begin(), str.end() };
}
#pragma warning( pop ) 

bool D3DApplication::ParseCommandLineArgs(LPWSTR argv[], int argc)
{
	if (argc <= 1)
		return true;

	// Hacky horrible wstring to string conversion
	// because the args library doesn't support wstring
	// and windows only gives wstring
	std::vector<std::string> args;
	for (int i = 1; i < argc; i++)
	{
		args.emplace_back(wstring_to_utf8(std::wstring(argv[i])));
	}

	args::ArgumentParser parser("Volumetrics in DirectX");
	args::HelpFlag help(parser, "help", "Display this help menu", { "help" });

	args::Group windowFlags(parser, "Window Flags");
	args::Flag fullscreen(windowFlags, "Fullscreen", "Start application in fullscreen", { 'f', "fullscreen" });
	args::Flag noGUI(windowFlags, "No GUI", "Disable GUI", { "no-gui" });

	args::Group graphicsContextFlags(parser, "Graphics Context Flags");
	args::Flag enablePIX(graphicsContextFlags, "Enable GPU Captures", "Enable PIX GPU Captures", { "enable-gpu-capture" });
	args::Flag enableDRED(graphicsContextFlags, "Enable DRED", "Enable Device Removal Extended Data", { "enable-dred" });

	args::Group applicationFlags(parser, "Application Flags");
	args::Flag orbitalCamera(applicationFlags, "Orbital Camera", "Enable an orbital camera", { "orbital-camera" });

#ifdef ENABLE_INSTRUMENTATION
	// These settings won't do anything in a non-instrumented build
	args::Command profilingCommand(parser, "profile", "Launch application in profiling mode", [this](args::Subparser& subparser)
		{
			this->m_ProfilingDataCollector->ParseCommandLineArgs(subparser);
		});
#endif

	try
	{
		parser.ParseCLI(args);
	}
	catch (const args::Help&)
	{
		LOG_INFO(parser.Help());
		return false;
	}
	catch (const args::ParseError& e)
	{
		LOG_ERROR(e.what());
		return false;
	}
	catch (const args::ValidationError& e)
	{
		LOG_ERROR(e.what());
		return false;
	}
	catch (const std::exception& e)
	{
		LOG_ERROR(e.what());
		return false;
	}

	if (fullscreen)
		m_ToggleFullscreen = true;
	if (noGUI)
		m_DisableGUI = true;

	if (enablePIX)
		m_GraphicsContextFlags.EnableGPUCaptures = true;
	if (enableDRED)
		m_GraphicsContextFlags.EnableDRED = true;

	if (orbitalCamera)
		m_UseOrbitalCamera = true;

	return true;
}


void D3DApplication::OnInit()
{
	LOG_INFO("Application startup.");

	// Create graphics context
	m_GraphicsContext = std::make_unique<D3DGraphicsContext>(Win32Application::GetHwnd(), GetWidth(), GetHeight(), m_GraphicsContextFlags);

	m_TextureLoader = std::make_unique<TextureLoader>();

	// SetScene camera
	m_Camera.SetPosition(XMVECTOR{ 0.0f, 2.0f, -10.0f });
	m_Timer.Reset();

	if (m_UseOrbitalCamera)
		m_CameraController = std::make_unique<OrbitalCameraController>(m_InputManager.get(), &m_Camera);
	else
		m_CameraController = std::make_unique<CameraController>(m_InputManager.get(), &m_Camera);

	m_PrevView = m_Camera.GetViewMatrix();
	m_PrevProj = m_Camera.GetProjectionMatrix();

	InitImGui();

	m_DeferredRenderer = std::make_unique<DeferredRenderer>();

	m_LightManager = std::make_unique<LightManager>();
	m_MaterialManager = std::make_unique<MaterialManager>(4);

	m_Scene = std::make_unique<VolumetricScene>(this, 1024);

	m_DeferredRenderer->SetScene(*m_Scene, *m_LightManager, *m_MaterialManager);

	// Load environment map
	{
		TextureLoader::LoadTextureConfig config = {};
		config.DesiredFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		config.DesiredChannels = 4;
		config.BytesPerChannel = 1;
		config.ResourceState = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

		// This will all take a very long time...
		auto environmentMap = m_TextureLoader->LoadTextureCubeFromFile("assets/textures/environment.png", &config, L"EnvironmentMap");

		m_TextureLoader->PerformUploadsImmediatelyAndBlock();
		m_LightManager->GetIBL()->ProcessEnvironmentMap(std::move(environmentMap));
		m_LightManager->GetIBL()->ProjectIrradianceMapToSH();
	}

	LOG_INFO("Application startup complete.");
}

void D3DApplication::OnUpdate()
{
	if (m_ToggleFullscreen)
	{
		m_ToggleFullscreen = false;
		Win32Application::ToggleFullscreenWindow(m_GraphicsContext->GetSwapChain());
	}

	PIXBeginEvent(PIX_COLOR_INDEX(0), L"App Update");
	BeginUpdate();

	float deltaTime = m_Timer.GetDeltaTime();

#ifdef ENABLE_INSTRUMENTATION
	// Update profiling before updating the scene, as this may modify the current scene
	m_ProfilingDataCollector->UpdateProfiling(m_Scene.get(), &m_Timer, m_CameraController.get());
#endif

	if (m_InputManager->IsKeyPressed(KEY_SPACE))
	{
		m_Paused = !m_Paused;
	}

	bool stepOnce = false;
	float timeDirection = 1.0f;
	if (m_Paused)
	{
		if (m_InputManager->IsKeyPressed(KEY_RIGHT))
		{
			stepOnce = true;
		}
		else if (m_InputManager->IsKeyPressed(KEY_LEFT))
		{
			stepOnce = true;
			timeDirection = -1.0f;
		}
	}

	deltaTime = deltaTime * m_TimeScale * timeDirection * static_cast<float>(!m_Paused || stepOnce);

	m_Scene->OnUpdate(deltaTime);

	if (m_ShowMainMenuBar && !m_DisableGUI)
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("Window"))
			{
				ImGui::MenuItem("Application Info", nullptr, &m_ShowApplicationInfo);
				ImGui::Separator();
				ImGui::MenuItem("Show Menu", nullptr, &m_ShowMainMenuBar);
	
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
		if (m_ShowApplicationInfo)
			m_ShowApplicationInfo = ImGuiApplicationInfo();
		if (m_ShowSceneInfo)
			m_ShowSceneInfo = m_Scene->DisplayGeneralGui();
		if (m_ShowSceneControls)
			m_ShowSceneControls = m_Scene->DisplayGui();
	}

	EndUpdate();
	PIXEndEvent();
}

void D3DApplication::OnRender()
{
	PIXBeginEvent(PIX_COLOR_INDEX(1), L"App Render");

	// Update constant buffer
	UpdatePassCB();
	m_LightManager->UpdateLightingCB(m_Camera.GetPosition());

	m_MaterialManager->UploadMaterialData();
	m_LightManager->CopyStagingBuffers();

	// Perform all queued uploads
	m_TextureLoader->PerformUploads();

	PROFILE_DIRECT_BEGIN_PASS("Frame");

	// Begin drawing
	m_GraphicsContext->StartDraw(m_PassCB);


	// Tell the scene that render is happening
	// This will update acceleration structures and other things to render the scene
	m_Scene->PreRender();

	// Render scene
	m_DeferredRenderer->Render();

	// Copy output from deferred renderer to back buffer
	ID3D12Resource* outputResource = m_DeferredRenderer->GetOutputResource();
	D3D12_RESOURCE_STATES outputResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	/*
	switch(m_GBufferDebugView)
	{
	case GB_DBG_Albedo:
		outputResource = m_DeferredRenderer->GetGBufferResource(0);
		outputResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		break;
	case GB_DBG_Normal:
		outputResource = m_DeferredRenderer->GetGBufferResource(1);
		outputResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		break;
	case GB_DBG_RoughnessMetalness:
		outputResource = m_DeferredRenderer->GetGBufferResource(2);
		outputResourceState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		break;
	case GB_DBG_Depth:
		outputResource = m_DeferredRenderer->GetDepthBufferResource();
		outputResourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;
		break;
	case GB_DBG_None:
	default:
		break;
	}
	*/
	m_GraphicsContext->CopyToBackBuffer(outputResource, outputResourceState);

	m_GraphicsContext->SetRenderTargetToBackBuffer(true);

	// ImGui Render
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), m_GraphicsContext->GetCommandList());

	// End draw
	m_GraphicsContext->EndDraw();

	PROFILE_DIRECT_END_PASS();

	// For multiple ImGui viewports
	const ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault(nullptr, m_GraphicsContext->GetCommandList());
	}

	m_GraphicsContext->Present();
	PIXEndEvent();
}

void D3DApplication::OnDestroy()
{
	m_GraphicsContext->WaitForGPUIdle();

	m_Scene.reset();

	m_DeferredRenderer.reset();

	m_LightManager.reset();
	m_MaterialManager.reset();

	m_TextureLoader.reset();

#ifdef ENABLE_INSTRUMENTATION
	GPUProfiler::Destroy();
#endif

	// Release graphics context
	m_GraphicsContext.reset();

	// Cleanup ImGui
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}


void D3DApplication::UpdatePassCB()
{
	// Calculate view matrix
	const XMMATRIX view = m_Camera.GetViewMatrix();
	const XMMATRIX proj = m_Camera.GetProjectionMatrix();

	const XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	const XMMATRIX invView = XMMatrixInverse(nullptr, view);
	const XMMATRIX invProj = XMMatrixInverse(nullptr, proj);
	const XMMATRIX invViewProj = XMMatrixInverse(nullptr, viewProj);

	// Update data in main pass constant buffer
	m_PassCB.View = XMMatrixTranspose(view);
	m_PassCB.InvView = XMMatrixTranspose(invView);
	m_PassCB.Proj = XMMatrixTranspose(proj);
	m_PassCB.InvProj = XMMatrixTranspose(invProj);
	m_PassCB.ViewProj = XMMatrixTranspose(viewProj);
	m_PassCB.InvViewProj = XMMatrixTranspose(invViewProj);

	m_PassCB.PrevView = XMMatrixTranspose(m_PrevView);
	m_PassCB.PrevProj = XMMatrixTranspose(m_PrevProj);
	m_PassCB.PrevViewProj = XMMatrixTranspose(XMMatrixMultiply(m_PrevView, m_PrevProj));

	m_PassCB.WorldEyePos = m_Camera.GetPosition();

	m_PassCB.RTSize = XMUINT2(m_Width, m_Height);
	m_PassCB.InvRTSize = XMFLOAT2(1.0f / static_cast<float>(m_Width), 1.0f / static_cast<float>(m_Height));

	const float n = m_Camera.GetNearPlane();
	const float f = m_Camera.GetFarPlane();

	m_PassCB.NearPlane = n;
	m_PassCB.FarPlane = f;

	// Coefficients required to transform view-space depth to NDC depth
	// computed as NDCDepth = A - B / ViewDepth
	m_PassCB.ViewDepthToNDC = {
		/* A = */ f / (f - n),
		/* B = */ f * n / (f - n)
	};

	m_PassCB.TotalTime = m_Timer.GetTimeSinceReset();
	m_PassCB.DeltaTime = m_Timer.GetDeltaTime();

	m_PassCB.FrameIndexMod16 = static_cast<UINT>(m_GraphicsContext->GetTotalFrameCount() % 16);

	// Set the previous view and proj to the new view and proj
	m_PrevView = view;
	m_PrevProj = proj;
}


void D3DApplication::InitImGui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	for (int fontSize = m_MinFontSize; fontSize <= m_MaxFontSize; fontSize++)
		m_Fonts[fontSize] = io.Fonts->AddFontFromFileTTF("assets/fonts/Cousine-Regular.ttf", static_cast<float>(fontSize));
	io.FontDefault = m_Fonts[m_FontSize];

	// SetScene platform and renderer back-ends
	ImGui_ImplWin32_Init(Win32Application::GetHwnd());
	ImGui_ImplDX12_Init(m_GraphicsContext->GetDevice(),
		m_GraphicsContext->GetBackBufferCount(),
		m_GraphicsContext->GetBackBufferFormat(),
		m_GraphicsContext->GetImGuiResourcesHeap(),
		m_GraphicsContext->GetImGuiCPUDescriptor(),
		m_GraphicsContext->GetImGuiGPUDescriptor());
}


void D3DApplication::BeginUpdate() 
{
	const float deltaTime = m_Timer.Tick();
	m_InputManager->Update(deltaTime);

	if (m_InputManager->IsKeyPressed(KEY_F11))
		m_ShowMainMenuBar = true;

	// Update camera
	m_CameraController->Update(deltaTime);

	// Begin new ImGui frame
	ImGuiIO& io = ImGui::GetIO();
	ASSERT(m_Fonts.find(m_FontSize) != m_Fonts.end(), "Font size not loaded!");
	io.FontDefault = m_Fonts[m_FontSize];

	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// Make viewport dock-able
	ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
}

void D3DApplication::EndUpdate()
{
	ImGui::Render();
	m_InputManager->EndFrame();
}


bool D3DApplication::ImGuiApplicationInfo()
{
	bool open = true;
	if (ImGui::Begin("Application", &open))
	{
		ImGui::Separator();
		{
			ImGui::LabelText("FPS", "%.1f", m_Timer.GetFPS());

			bool vsync = m_GraphicsContext->GetVSyncEnabled();
			if (ImGui::Checkbox("VSync", &vsync))
				m_GraphicsContext->SetVSyncEnabled(vsync);

			if (ImGui::InputInt("Font Size", &m_FontSize))
			{
				m_FontSize = min(max(m_FontSize, m_MinFontSize), m_MaxFontSize);
			}
		}
		ImGui::Separator();
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
			ImGui::Text("Camera");
			ImGui::PopStyleColor();

			ImGui::Separator();

			auto camPos = m_Camera.GetPosition();
			auto camYaw = m_Camera.GetYaw();
			auto camPitch = m_Camera.GetPitch();

			if (ImGui::DragFloat3("Position:", &camPos.x, 0.01f))
				m_Camera.SetPosition(camPos);
			if (ImGui::SliderAngle("Yaw:", &camYaw, -180.0f, 180.0f))
				m_Camera.SetYaw(camYaw);
			if (ImGui::SliderAngle("Pitch:", &camPitch, -89.0f, 89.0f))
				m_Camera.SetPitch(camPitch);

			ImGui::Separator();

			m_CameraController->Gui();
		}
		ImGui::Separator();

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Renderer");
		ImGui::PopStyleColor();

		ImGui::Separator();
		{
			m_DeferredRenderer->DrawGui();
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Lighting"))
		{
			m_LightManager->DrawGui();
		}

		ImGui::Separator();
		if (ImGui::CollapsingHeader("Materials"))
		{
			m_MaterialManager->DrawGui();
		}

		ImGui::Separator();

		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(ImColor(255, 255, 0)));
		ImGui::Text("Debug");
		ImGui::PopStyleColor();

		ImGui::Separator();

		{
			GuiHelpers::DisableScope disable(!m_GraphicsContext->IsPIXEnabled());

			if (ImGui::Button("GPU Capture"))
			{
				D3DDebugTools::PIXGPUCaptureFrame(1);
			}
		}
	}
	ImGui::End();
	return open;
}



void D3DApplication::OnResized()
{
	m_GraphicsContext->Resize(m_Width, m_Height);
	m_DeferredRenderer->OnBackBufferResize();

	m_Camera.SetAspectRatio(static_cast<float>(m_Width) / static_cast<float>(m_Height));
}


bool D3DApplication::GetTearingSupport() const
{
	return g_D3DGraphicsContext && g_D3DGraphicsContext->GetTearingSupport();
}
IDXGISwapChain* D3DApplication::GetSwapChain() const
{
	if (g_D3DGraphicsContext)
		return g_D3DGraphicsContext->GetSwapChain();
	return nullptr;
}
