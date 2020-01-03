#pragma once

#define _CRT_SECURE_NO_WARNINGS

#include <vector>
#include <string>
#include <list>
#include <queue>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <atomic>
#include <functional>
#include <iostream>
#include <chrono>
#include <cassert>

namespace fs = std::filesystem;

// Windows.
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// DirectX.
#include <dx/d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <wrl.h> 
using namespace Microsoft::WRL;

#undef near
#undef far
