//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

// pch.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently.

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
	#define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers.
#endif

// MUST BE INCLUDED BEFORE WINDOWS SDK/GRAPHICS API !!!
#include <directx/d3dx12.h>

#include <windows.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include "DirectXMath/SHMath/DirectXSH.h"
#include "DirectXTex/DirectXTex.h"

#include <string>
#include <vector>
#include <map>
#include <set>
#include <array>
#include <queue>
#include <deque>

#include <wrl.h>
#include <shellapi.h>
