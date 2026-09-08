#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <d3d12.h>
#include "externals/DirectXTex/DirectXTex.h"
#include <filesystem>
#include <limits>
#include <string>
#include <thread>
#include <unordered_map>
#include <wrl.h>

class DirectXCommon;
class SrvManager;

class Texture2DHandle final {
public:
    static constexpr uint32_t kInvalidIndex = (std::numeric_limits<uint32_t>::max)();
    constexpr Texture2DHandle() noexcept = default;

    [[nodiscard]] constexpr bool IsValid() const noexcept { return index_ != kInvalidIndex; }
    [[nodiscard]] constexpr uint32_t Index() const noexcept { return index_; }
	[[nodiscard]] constexpr uint32_t Generation() const noexcept { return generation_; }

    friend constexpr bool operator==(Texture2DHandle, Texture2DHandle) noexcept = default;

private:
    explicit constexpr Texture2DHandle(uint32_t index, uint32_t generation) noexcept
        : index_(index), generation_(generation) {}

    uint32_t index_ = kInvalidIndex;
	uint32_t generation_ = 0;
    friend class TextureManager;
};

class TextureCubeHandle final {
public:
    static constexpr uint32_t kInvalidIndex = (std::numeric_limits<uint32_t>::max)();
    constexpr TextureCubeHandle() noexcept = default;

    [[nodiscard]] constexpr bool IsValid() const noexcept { return index_ != kInvalidIndex; }
    [[nodiscard]] constexpr uint32_t Index() const noexcept { return index_; }
	[[nodiscard]] constexpr uint32_t Generation() const noexcept { return generation_; }

    friend constexpr bool operator==(TextureCubeHandle, TextureCubeHandle) noexcept = default;

private:
    explicit constexpr TextureCubeHandle(uint32_t index, uint32_t generation) noexcept
        : index_(index), generation_(generation) {}

    uint32_t index_ = kInvalidIndex;
	uint32_t generation_ = 0;
    friend class TextureManager;
};

class TextureManager final {
public:
	enum class Lifetime : uint8_t {
		Scene,
		Global,
	};

    struct Statistics {
        std::size_t texture2DCount = 0;
        std::size_t textureCubeCount = 0;
        uint64_t estimatedGpuBytes = 0;
    };

    static TextureManager* GetInstance();

    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;
    TextureManager(TextureManager&&) = delete;
    TextureManager& operator=(TextureManager&&) = delete;

    // Initialize/Finalize are restricted to the engine owner thread. Texture loads currently
    // complete synchronously on that same thread so resource visibility is deterministic.
    bool Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Finalize();

    [[nodiscard]] Texture2DHandle LoadTexture2D(
		const std::string& filePath,
		Lifetime lifetime = Lifetime::Scene);
    [[nodiscard]] TextureCubeHandle LoadTextureCube(
		const std::string& filePath,
		Lifetime lifetime = Lifetime::Scene);
    [[nodiscard]] Texture2DHandle CreateSolidColorTexture2D(
        const std::string& assetName,
        const std::array<uint8_t, 4>& color);

    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(Texture2DHandle handle) const;
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(TextureCubeHandle handle) const;
    [[nodiscard]] D3D12_RESOURCE_DESC GetResourceDesc(Texture2DHandle handle) const;

    [[nodiscard]] Texture2DHandle GetFallback2D() const noexcept { return fallback2D_; }
    [[nodiscard]] TextureCubeHandle GetFallbackCube() const noexcept { return fallbackCube_; }
    [[nodiscard]] Statistics GetStatistics() const noexcept;
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }
	void ReleaseSceneTextures();

private:
    enum class TextureKind : uint8_t {
        Texture2D,
        TextureCube,
    };

    struct TextureRecord {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_RESOURCE_DESC desc{};
        std::filesystem::path sourcePath;
        TextureKind kind = TextureKind::Texture2D;
		Lifetime lifetime = Lifetime::Scene;
		uint32_t generation = 0;
        uint64_t estimatedGpuBytes = 0;
    };

    TextureManager() = default;
    ~TextureManager();

    [[nodiscard]] bool IsOwnerThread() const noexcept;
    [[nodiscard]] std::wstring MakeCacheKey(const std::filesystem::path& filePath) const;
    [[nodiscard]] bool LoadImage(
        const std::filesystem::path& filePath,
        TextureKind expectedKind,
        DirectX::ScratchImage& outImage) const;
    [[nodiscard]] bool CreateFallbackTextures();
    [[nodiscard]] bool RegisterTexture(
        DirectX::ScratchImage& image,
        const std::filesystem::path& sourcePath,
        TextureKind kind,
		Lifetime lifetime,
        uint32_t& outSrvIndex);
    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource(
        const DirectX::TexMetadata& metadata) const;
    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadBuffer(uint64_t sizeInBytes) const;
    [[nodiscard]] bool UploadTexture(
        ID3D12Resource* texture,
        const DirectX::ScratchImage& image);
    [[nodiscard]] const TextureRecord* FindRecord(
		uint32_t descriptorIndex,
		uint32_t generation,
		TextureKind kind) const;
	[[nodiscard]] TextureRecord* FindRecord(
		uint32_t descriptorIndex,
		uint32_t generation,
		TextureKind kind);
    [[nodiscard]] uint64_t EstimateGpuBytes(const DirectX::TexMetadata& metadata) const noexcept;
    void LogFailure(const std::string& operation, HRESULT result) const;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::thread::id ownerThread_{};
    bool initialized_ = false;

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> uploadCommandAllocator_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> uploadCommandList_;
    Microsoft::WRL::ComPtr<ID3D12Fence> uploadFence_;
    HANDLE uploadFenceEvent_ = nullptr;
    uint64_t uploadFenceValue_ = 0;

    std::unordered_map<std::wstring, Texture2DHandle> texture2DCache_;
    std::unordered_map<std::wstring, TextureCubeHandle> textureCubeCache_;
    std::unordered_map<uint32_t, TextureRecord> records_;
	std::unordered_map<uint32_t, uint32_t> descriptorGenerations_;

    Texture2DHandle fallback2D_{};
    TextureCubeHandle fallbackCube_{};
    uint64_t estimatedGpuBytes_ = 0;
};
