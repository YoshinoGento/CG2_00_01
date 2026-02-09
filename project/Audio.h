#pragma once
#include <Windows.h>
#include <wrl.h>
#include <xaudio2.h>
#include <string>
#include <vector>
#include <map>

// 音声データを管理する構造体
struct SoundData {
	// 波形フォーマット
	WAVEFORMATEX wfex;
	// バッファの先頭アドレス
	std::vector<BYTE> pBuffer;
	// バッファのサイズ
	unsigned int bufferSize;
};

class Audio {
public:
	// 初期化
	void Initialize();

	// 終了処理
	void Finalize();

	// 音声データの読み込み (.wav)
	uint32_t LoadWave(const std::string& filename);

	// 音声再生
	void PlayWave(uint32_t soundHandle, bool loop = false, float volume = 1.0f);

	// シングルトン化する場合や、WinAppのように管理クラスとして扱うならこれらは不要ですが、
	// 今回は main.cpp で unique_ptr 管理する形にします。

private:
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_;
	IXAudio2MasteringVoice* masterVoice_ = nullptr;

	// 読み込んだサウンドデータを保存しておくコンテナ
	std::map<uint32_t, SoundData> soundDatas_;
	// 次に使うハンドル番号
	uint32_t nextHandle_ = 0u;
};