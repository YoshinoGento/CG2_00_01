// --- 定数定義 ---
#define _USE_MATH_DEFINES

// --- Windows / 標準ライブラリ ---
#include <Windows.h>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <strsafe.h>
#include <vector>
#include <numbers>
#include <cmath> 

// --- Direct3D 12 / DXGI 関連 ---
#include <d3d12.h>
#include <dxgi1_6.h>
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")

// --- DirectX デバッグ支援 ---
#include <dbghelp.h>
#include <dxgidebug.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "dxguid.lib")

// --- DXC (Shader Compiler) ---
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")

// --- DirectXTex ---
// d3dx12.hは使わず標準機能だけで実装します
#include "externals/DirectXTex/DirectXTex.h"

// --- ImGui ---
#include "externals/imgui/imgui.h"
#include "externals/imgui/imgui_impl_dx12.h"
#include "externals/imgui/imgui_impl_win32.h"

// --- 自作ヘッダ ---
#include "Struct.h"
#include "Matrix.h"
#include "Transform.h"
#include "ResourceObject.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wparam, lparam)) {
		return true;
	}
	switch (msg) {
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);
}

// 文字列変換
std::wstring ConvertString(const std::string& str) {
	if (str.empty()) return std::wstring();
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
	std::wstring wstrTo(size_needed, 0);
	MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
	return wstrTo;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) return std::string();
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0, NULL, NULL);
	std::string strTo(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, &str[0], (int)str.size(), &strTo[0], size_needed, NULL, NULL);
	return strTo;
}

void Log(std::ofstream& logStream, const std::string& message) {
	logStream << message;
	OutputDebugStringA(message.c_str());
}

// 頂点データ構造体
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// マテリアル構造体
struct Material {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
};

// 座標変換行列構造体
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

// 平行光源構造体
struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};

// カメラ座標構造体
struct CameraForGPU {
	Vector3 worldPosition;
};


// リソース作成関数
ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes) {
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE,
		&resourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

// DepthStencilTextureを作る
ID3D12Resource* CreateDepthStencilTextureResource(ID3D12Device* device, int32_t width, int32_t height) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;
	resourceDesc.Height = height;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthClearValue, IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

// ------------------------------------------------------------------------------------------------
// ヘルパー関数の自作実装（d3dx12.hのエラー回避）
// ------------------------------------------------------------------------------------------------
UINT64 GetRequiredIntermediateSize(ID3D12Resource* pDestinationResource, UINT FirstSubresource, UINT NumSubresources) {
	D3D12_RESOURCE_DESC Desc = pDestinationResource->GetDesc();
	UINT64 RequiredSize = 0;
	ID3D12Device* pDevice = nullptr;
	pDestinationResource->GetDevice(IID_PPV_ARGS(&pDevice));
	pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, 0, nullptr, nullptr, nullptr, &RequiredSize);
	pDevice->Release();
	return RequiredSize;
}

void UpdateSubresources(ID3D12GraphicsCommandList* pCmdList, ID3D12Resource* pDestinationResource, ID3D12Resource* pIntermediate, UINT64 IntermediateOffset, UINT FirstSubresource, UINT NumSubresources, D3D12_SUBRESOURCE_DATA* pSrcData) {
	D3D12_RESOURCE_DESC Desc = pDestinationResource->GetDesc();
	ID3D12Device* pDevice = nullptr;
	pDestinationResource->GetDevice(IID_PPV_ARGS(&pDevice));

	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> Layouts(NumSubresources);
	std::vector<UINT> NumRows(NumSubresources);
	std::vector<UINT64> RowSizesInBytes(NumSubresources);
	UINT64 RequiredSize = 0;

	pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, IntermediateOffset, Layouts.data(), NumRows.data(), RowSizesInBytes.data(), &RequiredSize);
	pDevice->Release();

	BYTE* pData;
	HRESULT hr = pIntermediate->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	if (FAILED(hr)) return;

	for (UINT i = 0; i < NumSubresources; ++i) {
		D3D12_SUBRESOURCE_DATA srcData = pSrcData[i];
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = Layouts[i];
		UINT numRows = NumRows[i];
		UINT64 rowSize = RowSizesInBytes[i];

		BYTE* pDestSubresource = pData + layout.Offset;
		const BYTE* pSrcSubresource = reinterpret_cast<const BYTE*>(srcData.pData);

		for (UINT y = 0; y < numRows; ++y) {
			memcpy(pDestSubresource + y * layout.Footprint.RowPitch, pSrcSubresource + y * srcData.RowPitch, static_cast<size_t>(rowSize));
		}
	}
	pIntermediate->Unmap(0, nullptr);

	for (UINT i = 0; i < NumSubresources; ++i) {
		D3D12_TEXTURE_COPY_LOCATION Dst{};
		Dst.pResource = pDestinationResource;
		Dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		Dst.SubresourceIndex = FirstSubresource + i;

		D3D12_TEXTURE_COPY_LOCATION Src{};
		Src.pResource = pIntermediate;
		Src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		Src.PlacedFootprint = Layouts[i];

		pCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
	}
}
// ------------------------------------------------------------------------------------------------


// ★Textureをロードする関数（ファイルが見つからなければ市松模様を作る）
DirectX::ScratchImage LoadTexture(const std::string& filePath) {
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString(filePath);

	// まずWICで読み込みを試みる
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);

	// 失敗した場合、別のパスも試す (ファイル名だけとか)
	if (FAILED(hr)) {
		std::filesystem::path path(filePath);
		std::wstring filename = path.filename().wstring();
		hr = DirectX::LoadFromWICFile(filename.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	}

	// それでも失敗した場合は「ピンクと白の市松模様」を生成して返す
	if (FAILED(hr)) {
		OutputDebugStringA("Texture Load Failed! Using Fallback Texture.\n");
		DirectX::TexMetadata metadata{};
		metadata.width = 64; metadata.height = 64; metadata.depth = 1; metadata.arraySize = 1;
		metadata.mipLevels = 1; metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		metadata.dimension = DirectX::TEX_DIMENSION_TEXTURE2D;

		image.Initialize(metadata, DirectX::CP_FLAGS_NONE);
		uint8_t* pixels = image.GetPixels();

		// 64x64の市松模様
		for (int y = 0; y < 64; ++y) {
			for (int x = 0; x < 64; ++x) {
				int index = (y * 64 + x) * 4;
				// 8ピクセルごとに色を変える
				if (((x / 8) % 2) == ((y / 8) % 2)) {
					// 白
					pixels[index + 0] = 255;
					pixels[index + 1] = 255;
					pixels[index + 2] = 255;
					pixels[index + 3] = 255;
				} else {
					// マゼンタ (エラーっぽく目立つ色)
					pixels[index + 0] = 255; // R
					pixels[index + 1] = 0;   // G
					pixels[index + 2] = 255; // B
					pixels[index + 3] = 255; // A
				}
			}
		}
		return image;
	}

	// 成功したらミップマップ生成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	if (FAILED(hr)) {
		return image;
	}
	return mipImages;
}

// TextureResourceを作る関数
ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metadata) {
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);
	resourceDesc.Height = UINT(metadata.height);
	resourceDesc.MipLevels = UINT16(metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16(metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource));
	assert(SUCCEEDED(hr));
	return resource;
}

// Textureデータを転送する関数
ID3D12Resource* UploadTextureData(ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device, ID3D12GraphicsCommandList* commandList) {
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device, mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);

	UINT64 intermediateSize = GetRequiredIntermediateSize(texture, 0, UINT(subresources.size()));
	ID3D12Resource* intermediateResource = CreateBufferResource(device, intermediateSize);

	UpdateSubresources(commandList, texture, intermediateResource, 0, 0, UINT(subresources.size()), subresources.data());

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Transition.pResource = texture;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	commandList->ResourceBarrier(1, &barrier);

	return intermediateResource;
}

// ディスクリプタハンドル取得関数
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

// ダミーエラー出力用
LONG WINAPI ExportDump(EXCEPTION_POINTERS* exceptionPointers) {
	OutputDebugStringA("Error occurred!\n");
	return EXCEPTION_EXECUTE_HANDLER;
}


// ★球の頂点生成関数
void GenerateSphereVertices(VertexData* vertices, int kSubdivision, float radius) {
	const float kLonEvery = static_cast<float>(std::numbers::pi * 2.0f) / kSubdivision;
	const float kLatEvery = static_cast<float>(std::numbers::pi) / kSubdivision;

	for (int latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		float lat = -static_cast<float>(std::numbers::pi) / 2.0f + kLatEvery * latIndex;
		float nextLat = lat + kLatEvery;

		for (int lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			float lon = kLonEvery * lonIndex;
			float nextLon = lon + kLonEvery;

			VertexData vertA, vertB, vertC, vertD;
			vertA.position = { cosf(lat) * cosf(lon) * radius, sinf(lat) * radius, cosf(lat) * sinf(lon) * radius, 1.0f };
			vertA.texcoord = { static_cast<float>(lonIndex) / kSubdivision, 1.0f - static_cast<float>(latIndex) / kSubdivision };
			vertA.normal = { cosf(lat) * cosf(lon), sinf(lat), cosf(lat) * sinf(lon) };

			vertB.position = { cosf(nextLat) * cosf(lon) * radius, sinf(nextLat) * radius, cosf(nextLat) * sinf(lon) * radius, 1.0f };
			vertB.texcoord = { static_cast<float>(lonIndex) / kSubdivision, 1.0f - static_cast<float>(latIndex + 1) / kSubdivision };
			vertB.normal = { cosf(nextLat) * cosf(lon), sinf(nextLat), cosf(nextLat) * sinf(lon) };

			vertC.position = { cosf(lat) * cosf(nextLon) * radius, sinf(lat) * radius, cosf(lat) * sinf(nextLon) * radius, 1.0f };
			vertC.texcoord = { static_cast<float>(lonIndex + 1) / kSubdivision, 1.0f - static_cast<float>(latIndex) / kSubdivision };
			vertC.normal = { cosf(lat) * cosf(nextLon), sinf(lat), cosf(lat) * sinf(nextLon) };

			vertD.position = { cosf(nextLat) * cosf(nextLon) * radius, sinf(nextLat) * radius, cosf(nextLat) * sinf(nextLon) * radius, 1.0f };
			vertD.texcoord = { static_cast<float>(lonIndex + 1) / kSubdivision, 1.0f - static_cast<float>(latIndex + 1) / kSubdivision };
			vertD.normal = { cosf(nextLat) * cosf(nextLon), sinf(nextLat), cosf(nextLat) * sinf(nextLon) };

			uint32_t startIndex = (latIndex * kSubdivision + lonIndex) * 6;

			vertices[startIndex + 0] = vertA;
			vertices[startIndex + 1] = vertB;
			vertices[startIndex + 2] = vertC;

			vertices[startIndex + 3] = vertC;
			vertices[startIndex + 4] = vertD;
			vertices[startIndex + 5] = vertB;
		}
	}
}


// Windowsアプリでのエントリーポイント
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
	CoInitializeEx(0, COINIT_MULTITHREADED);
	SetUnhandledExceptionFilter(ExportDump);
	std::filesystem::create_directory("logs");

	std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	std::chrono::zoned_time loacalTime{ std::chrono::current_zone(), nowSeconds };
	std::string dateString = std::format("{:%Y%m%d_%H%M%S}", loacalTime);
	std::string logFilePath = std::string("logs/") + dateString + ".log";
	std::ofstream logStream(logFilePath);

	WNDCLASS wc{};
	wc.lpfnWndProc = WindowProc;
	wc.lpszClassName = L"CG2WindowClass";
	wc.hInstance = GetModuleHandle(nullptr);
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	RegisterClass(&wc);

	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	HWND hwnd = CreateWindow(wc.lpszClassName, L"CG2_Shader", WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, wrc.right - wrc.left, wrc.bottom - wrc.top,
		nullptr, nullptr, wc.hInstance, nullptr);
	ShowWindow(hwnd, SW_SHOW);

#ifdef _DEBUG
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	IDXGIFactory7* dxgiFactory = nullptr;
	HRESULT hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	IDXGIAdapter4* useAdapter = nullptr;
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC3 adapterDesc{};
		useAdapter->GetDesc3(&adapterDesc);
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);
	ID3D12Device* device = nullptr;
	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_2, D3D_FEATURE_LEVEL_12_1, D3D_FEATURE_LEVEL_12_0 };
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) break;
	}
	assert(device != nullptr);

#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue)))) {
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		infoQueue->Release();
	}
#endif

	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));

	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;
	swapChainDesc.Height = kClientHeight;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));

	// ★修正点：ディスクリプタヒープの数を増やす (128)
	ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};
	rtvDescriptorHeapDesc.NumDescriptors = 2;
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	hr = device->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
	assert(SUCCEEDED(hr));

	ID3D12Resource* swapChainResources[2] = { nullptr };
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	for (int i = 0; i < 2; ++i) {
		hr = swapChain->GetBuffer(i, IID_PPV_ARGS(&swapChainResources[i]));
		assert(SUCCEEDED(hr));
		rtvHandles[i] = rtvStartHandle;
		rtvHandles[i].ptr += i * device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
		D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
		rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		device->CreateRenderTargetView(swapChainResources[i], &rtvDesc, rtvHandles[i]);
	}

	ID3D12CommandAllocator* commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	// Fence生成 (TextureUpload前に必要)
	ID3D12Fence* fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));
	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);


	// ==== ルートシグネチャ ====
	ID3D12RootSignature* rootSignature = nullptr;
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ★ルートパラメータ 5つ
	D3D12_ROOT_PARAMETER rootParameters[5] = {};
	// [0] Material
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;
	// [1] WVP
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;
	// [2] Texture
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;
	// [3] Light
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;
	// [4] Camera
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr)) {
		OutputDebugStringA(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	assert(SUCCEEDED(hr));

	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 16, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	rasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // カリングなし
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	// Shader Compile
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	IDxcIncludeHandler* includHandler = nullptr;
	DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	dxcUtils->CreateDefaultIncludeHandler(&includHandler);

	auto CompileShader = [&](const std::wstring& filename, const wchar_t* profile) {
		IDxcBlobEncoding* sourceBlob = nullptr;
		dxcUtils->LoadFile(filename.c_str(), nullptr, &sourceBlob);
		DxcBuffer sourceBuffer;
		sourceBuffer.Ptr = sourceBlob->GetBufferPointer();
		sourceBuffer.Size = sourceBlob->GetBufferSize();
		sourceBuffer.Encoding = DXC_CP_UTF8;
		LPCWSTR args[] = { filename.c_str(), L"-E", L"main", L"-T", profile, L"-Zi", L"-Qembed_debug", L"-Od", L"-Zpr" };
		IDxcResult* result = nullptr;
		dxcCompiler->Compile(&sourceBuffer, args, _countof(args), includHandler, IID_PPV_ARGS(&result));
		IDxcBlob* blob = nullptr;
		result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&blob), nullptr);
		IDxcBlobUtf8* errors = nullptr;
		result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr);
		if (errors && errors->GetStringLength() > 0) {
			OutputDebugStringA(errors->GetStringPointer());
			assert(false);
		}
		return blob;
		};

	IDxcBlob* vsBlob = CompileShader(L"Object3D.VS.hlsl", L"vs_6_0");
	IDxcBlob* psBlob = CompileShader(L"Object3D.PS.hlsl", L"ps_6_0");

	// DepthStencil State
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	// PSO
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
	psoDesc.pRootSignature = rootSignature;
	psoDesc.InputLayout = inputLayoutDesc;
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.BlendState = blendDesc;
	psoDesc.RasterizerState = rasterizerDesc;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	ID3D12PipelineState* pipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState));
	assert(SUCCEEDED(hr));

	// リソース作成
	const int kSubdivision = 16;
	const float kRadius = 2.0f;
	const uint32_t kSphereVertexCount = kSubdivision * kSubdivision * 6;
	std::vector<VertexData> sphereVertices(kSphereVertexCount);
	GenerateSphereVertices(sphereVertices.data(), kSubdivision, kRadius);

	ID3D12Resource* sphereVertexResource = CreateBufferResource(device, sizeof(VertexData) * kSphereVertexCount);
	D3D12_VERTEX_BUFFER_VIEW sphereVBV{};
	sphereVBV.BufferLocation = sphereVertexResource->GetGPUVirtualAddress();
	sphereVBV.SizeInBytes = sizeof(VertexData) * kSphereVertexCount;
	sphereVBV.StrideInBytes = sizeof(VertexData);
	VertexData* vertMap = nullptr;
	sphereVertexResource->Map(0, nullptr, (void**)&vertMap);
	memcpy(vertMap, sphereVertices.data(), sizeof(VertexData) * kSphereVertexCount);
	sphereVertexResource->Unmap(0, nullptr);

	// マテリアル
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Material));
	Material* materialData = nullptr;
	materialResource->Map(0, nullptr, (void**)&materialData);
	materialData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	materialData->enableLighting = 1;
	materialData->uvTransform = MatrixMath::MakeIdentity4x4();

	// WVP
	ID3D12Resource* sphereWvpResource = CreateBufferResource(device, sizeof(TransformationMatrix));
	TransformationMatrix* sphereWvpData = nullptr;
	sphereWvpResource->Map(0, nullptr, (void**)&sphereWvpData);
	sphereWvpData->WVP = MatrixMath::MakeIdentity4x4();
	sphereWvpData->World = MatrixMath::MakeIdentity4x4();

	// Light
	ID3D12Resource* lightResource = CreateBufferResource(device, sizeof(DirectionalLight));
	DirectionalLight* lightData = nullptr;
	lightResource->Map(0, nullptr, (void**)&lightData);
	lightData->color = { 1.0f, 1.0f, 1.0f, 1.0f };
	lightData->direction.x = 0.577f;
	lightData->direction.y = -0.577f;
	lightData->direction.z = 0.577f;
	lightData->intensity = 1.0f;

	// Camera
	ID3D12Resource* cameraResource = CreateBufferResource(device, sizeof(CameraForGPU));
	CameraForGPU* cameraData = nullptr;
	cameraResource->Map(0, nullptr, (void**)&cameraData);
	cameraData->worldPosition.x = 0.0f;
	cameraData->worldPosition.y = 0.0f;
	cameraData->worldPosition.z = -10.0f;

	// Depth
	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource(device, kClientWidth, kClientHeight);
	ID3D12DescriptorHeap* dsvHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, D3D12_DESCRIPTOR_HEAP_FLAG_NONE, 0 };
	device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap));
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvViewDesc{};
	dsvViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	device->CreateDepthStencilView(depthStencilResource, &dsvViewDesc, dsvHeap->GetCPUDescriptorHandleForHeapStart());

	// SRV Heap (Texture & ImGui)
	ID3D12DescriptorHeap* srvHeap = nullptr;
	// ★修正点: 数を128に増やして、ImGuiとTextureで場所を分けられるようにする
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE, 0 };
	device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvHeap));

	// ★★★ Textureのロードとアップロード ★★★
	// ファイルパスは適宜変更してください (resources/monsterBall.png など)
	DirectX::ScratchImage mipImages = LoadTexture("resources/monsterBall.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	// 2. Resource作成
	ID3D12Resource* textureResource = CreateTextureResource(device, metadata);

	// 3. アップロード
	ID3D12Resource* intermediateResource = UploadTextureData(textureResource, mipImages, device, commandList);

	// 4. SRV作成 (Heapの先頭を使う)
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);
	// ★SRVはヒープの先頭(index 0)に作成
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvHeap->GetGPUDescriptorHandleForHeapStart();
	device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);

	// ★★★ アップロードコマンドを実行 ★★★
	commandList->Close();
	ID3D12CommandList* ppCommandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, ppCommandLists);

	// 待機
	fenceValue++;
	commandQueue->Signal(fence, fenceValue);
	if (fence->GetCompletedValue() < fenceValue) {
		fence->SetEventOnCompletion(fenceValue, fenceEvent);
		WaitForSingleObject(fenceEvent, INFINITE);
	}

	// 完了したのでIntermediate Resourceは解放
	intermediateResource->Release();

	// コマンドリストをリセットしてメインループへ
	commandAllocator->Reset();
	commandList->Reset(commandAllocator, nullptr);


	// ImGuiの初期化
	// ★修正点: ImGuiが使うヒープの場所をずらす (index 1から使うようにする)
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui_ImplWin32_Init(hwnd);

	// ヒープのハンドルを1つ分ずらす
	UINT handleSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	D3D12_CPU_DESCRIPTOR_HANDLE fontSrvHandleCPU = textureSrvHandleCPU;
	fontSrvHandleCPU.ptr += handleSize;
	D3D12_GPU_DESCRIPTOR_HANDLE fontSrvHandleGPU = textureSrvHandleGPU;
	fontSrvHandleGPU.ptr += handleSize;

	ImGui_ImplDX12_Init(device, 2, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, srvHeap, fontSrvHandleCPU, fontSrvHandleGPU);

	Transform sphereTransform{ {1,1,1}, {0,0,0}, {0,0,0} };
	Transform cameraTransform{ {1,1,1}, {0.2f,0,0}, {0,2.0f,-10.0f} };

	MSG msg{};
	while (msg.message != WM_QUIT) {
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		} else {
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();

			ImGui::Begin("Debug");
			ImGui::DragFloat3("Light Dir", &lightData->direction.x, 0.01f, -1.0f, 1.0f);
			ImGui::DragFloat3("Sphere Pos", &sphereTransform.translate.x, 0.1f);
			ImGui::End();

			// Light Normalize
			float len = sqrt(lightData->direction.x * lightData->direction.x + lightData->direction.y * lightData->direction.y + lightData->direction.z * lightData->direction.z);
			if (len > 0) {
				lightData->direction.x /= len;
				lightData->direction.y /= len;
				lightData->direction.z /= len;
			}

			// Update Matrices
			Matrix4x4 camMat = MatrixMath::MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMat = MatrixMath::Inverse(camMat);
			Matrix4x4 projMat = MatrixMath::MakePerspectiveFovMatrix(0.45f, (float)kClientWidth / kClientHeight, 0.1f, 100.0f);
			Matrix4x4 vpMat = MatrixMath::Multiply(viewMat, projMat);
			Matrix4x4 worldMat = MatrixMath::MakeAffineMatrix(sphereTransform.scale, sphereTransform.rotate, sphereTransform.translate);
			sphereWvpData->World = worldMat;
			sphereWvpData->WVP = MatrixMath::Multiply(worldMat, vpMat);

			// Update Camera Pos
			cameraData->worldPosition = cameraTransform.translate;

			// Draw
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			commandList->ResourceBarrier(1, &barrier);

			D3D12_CPU_DESCRIPTOR_HANDLE rtvH = rtvHandles[backBufferIndex];
			D3D12_CPU_DESCRIPTOR_HANDLE dsvH = dsvHeap->GetCPUDescriptorHandleForHeapStart();
			commandList->OMSetRenderTargets(1, &rtvH, false, &dsvH);
			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
			commandList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
			commandList->ClearDepthStencilView(dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

			D3D12_VIEWPORT vp{ 0,0,(float)kClientWidth,(float)kClientHeight,0,1 };
			D3D12_RECT sc{ 0,0,kClientWidth,kClientHeight };
			commandList->RSSetViewports(1, &vp);
			commandList->RSSetScissorRects(1, &sc);

			commandList->SetGraphicsRootSignature(rootSignature);
			commandList->SetPipelineState(pipelineState);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			ID3D12DescriptorHeap* heaps[] = { srvHeap };
			commandList->SetDescriptorHeaps(1, heaps);

			commandList->IASetVertexBuffers(0, 1, &sphereVBV);
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(1, sphereWvpResource->GetGPUVirtualAddress());
			// ★テクスチャ(index 0)をセット
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);
			commandList->SetGraphicsRootConstantBufferView(3, lightResource->GetGPUVirtualAddress());
			commandList->SetGraphicsRootConstantBufferView(4, cameraResource->GetGPUVirtualAddress());

			commandList->DrawInstanced(kSphereVertexCount, 1, 0, 0);

			ImGui::Render();
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			commandList->ResourceBarrier(1, &barrier);
			commandList->Close();

			ID3D12CommandList* cmds[] = { commandList };
			commandQueue->ExecuteCommandLists(1, cmds);
			swapChain->Present(1, 0);

			fenceValue++;
			commandQueue->Signal(fence, fenceValue);
			if (fence->GetCompletedValue() < fenceValue) {
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				WaitForSingleObject(fenceEvent, INFINITE);
			}
			commandAllocator->Reset();
			commandList->Reset(commandAllocator, nullptr);
		}
	}

	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	CloseHandle(fenceEvent);
	fence->Release();
	commandList->Release();
	commandAllocator->Release();
	dsvHeap->Release();
	depthStencilResource->Release();
	rtvDescriptorHeap->Release();
	swapChain->Release();
	commandQueue->Release();
	device->Release();
	useAdapter->Release();
	dxgiFactory->Release();
#ifdef _DEBUG
	debugController->Release();
#endif

	rootSignature->Release();
	pipelineState->Release();
	vsBlob->Release();
	psBlob->Release();

	sphereVertexResource->Release();
	materialResource->Release();
	sphereWvpResource->Release();
	lightResource->Release();
	cameraResource->Release();
	srvHeap->Release();
	textureResource->Release();

	return 0;
}