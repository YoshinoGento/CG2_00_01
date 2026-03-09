#define NOMINMAX // Windows.hのmin/maxマクロと標準関数の衝突を防ぐ
#include "Audio.h"
#include <Windows.h>
#include <cassert>
#include <algorithm>

// Media Foundation 関連のヘッダー
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>

// ライブラリのリンク指定
#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfreadwrite.lib")
#pragma comment(lib, "mfuuid.lib")

/**
 * システムの初期化
 */
void Audio::Initialize() {
	// 1. XAudio2エンジンの作成
	HRESULT hr = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(hr));

	// 2. マスターボイス（PCのスピーカー出力先）の作成
	hr = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(hr));

	// 3. Media Foundationの初期化（圧縮音源のデコードに必要）
	hr = MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);
	assert(SUCCEEDED(hr));
}

/**
 * 終了処理
 */
void Audio::Finalize() {
	// 全ての再生機（SourceVoice）を安全に破棄
	for (auto& pair : soundDatas_) {
		if (pair.second.pSourceVoice) {
			pair.second.pSourceVoice->DestroyVoice();
		}
	}
	soundDatas_.clear();
	xAudio2_.Reset();

	// Media Foundationの終了
	MFShutdown();
}

/**
 * 音声ファイルの読み込み（Media Foundation を使用して PCM に変換）
 */
uint32_t Audio::LoadAudio(const std::string& filename) {
	// ファイルパスをワイド文字列（wstring）に変換
	std::wstring wfilename(filename.begin(), filename.end());

	// 1. ソースリーダーの作成（ファイルを開く）
	Microsoft::WRL::ComPtr<IMFSourceReader> pSourceReader;
	HRESULT hr = MFCreateSourceReaderFromURL(wfilename.c_str(), nullptr, &pSourceReader);
	assert(SUCCEEDED(hr) && "音声ファイルが開けません。パスを確認してください。");

	// 2. メディアタイプの選択（音声ストリームのみを使う）
	hr = pSourceReader->SetStreamSelection((uint32_t)MF_SOURCE_READER_ALL_STREAMS, false);
	hr = pSourceReader->SetStreamSelection((uint32_t)MF_SOURCE_READER_FIRST_AUDIO_STREAM, true);

	// 3. 出力形式を非圧縮PCM（生の音データ）に設定
	Microsoft::WRL::ComPtr<IMFMediaType> pPartialType;
	MFCreateMediaType(&pPartialType);
	pPartialType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	pPartialType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	hr = pSourceReader->SetCurrentMediaType((uint32_t)MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pPartialType.Get());

	// 4. 音情報の詳細（サンプリングレート等）を手動で取得
	// ※MFCreateWaveFormatExFromMediaType がエラーになる環境があるため、直接属性から抜き取ります
	Microsoft::WRL::ComPtr<IMFMediaType> pOutputType;
	hr = pSourceReader->GetCurrentMediaType((uint32_t)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pOutputType);

	WAVEFORMATEX wfex = {};
	GUID subType;
	pOutputType->GetGUID(MF_MT_SUBTYPE, &subType);
	pOutputType->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, (UINT32*)&wfex.nChannels);
	pOutputType->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32*)&wfex.nSamplesPerSec);
	pOutputType->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, (UINT32*)&wfex.wBitsPerSample);
	wfex.wFormatTag = WAVE_FORMAT_PCM;
	wfex.nBlockAlign = wfex.nChannels * wfex.wBitsPerSample / 8;
	wfex.nAvgBytesPerSec = wfex.nSamplesPerSec * wfex.nBlockAlign;

	// 5. データを最後まで読み込んでバッファ（std::vector）に溜める
	std::vector<byte> audioData;
	while (true) {
		Microsoft::WRL::ComPtr<IMFSample> pSample;
		DWORD dwFlags = 0;
		hr = pSourceReader->ReadSample((uint32_t)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &dwFlags, nullptr, &pSample);

		if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) break; // ファイル終了
		if (pSample == nullptr) continue;

		// サンプルデータをメモリにコピー
		Microsoft::WRL::ComPtr<IMFMediaBuffer> pBuffer;
		pSample->ConvertToContiguousBuffer(&pBuffer);

		byte* pAudioBuffer = nullptr;
		DWORD cbCurrentLength = 0;
		pBuffer->Lock(&pAudioBuffer, nullptr, &cbCurrentLength);

		size_t oldSize = audioData.size();
		audioData.resize(oldSize + cbCurrentLength);
		std::memcpy(&audioData[oldSize], pAudioBuffer, cbCurrentLength);

		pBuffer->Unlock();
	}

	// 6. 管理マップにデータを保存
	uint32_t handle = nextHandle_++;
	SoundData data = {};
	data.wfex = wfex;
	data.pBuffer = std::move(audioData);
	data.bufferSize = (uint32_t)data.pBuffer.size();
	data.pSourceVoice = nullptr; // 再生するまでボイスは作らない
	soundDatas_[handle] = std::move(data);

	return handle;
}

/**
 * 音声の再生
 */
void Audio::PlayWave(uint32_t handle, bool loop) {
	auto it = soundDatas_.find(handle);
	if (it == soundDatas_.end()) return;

	SoundData& soundData = it->second;

	// 再生中ならリセット
	if (soundData.pSourceVoice) {
		soundData.pSourceVoice->Stop();
		soundData.pSourceVoice->FlushSourceBuffers();
		soundData.pSourceVoice->DestroyVoice();
		soundData.pSourceVoice = nullptr;
	}

	// 再生機の作成
	HRESULT hr = xAudio2_->CreateSourceVoice(&soundData.pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(hr));

	// バッファ設定
	XAUDIO2_BUFFER buffer = {};
	buffer.pAudioData = soundData.pBuffer.data();
	buffer.AudioBytes = soundData.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;
	if (loop) {
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	// 再生開始！
	hr = soundData.pSourceVoice->SubmitSourceBuffer(&buffer);
	assert(SUCCEEDED(hr));
	hr = soundData.pSourceVoice->Start();
	assert(SUCCEEDED(hr));
}

/**
 * 音声の停止
 */
void Audio::StopWave(uint32_t handle) {
	auto it = soundDatas_.find(handle);
	if (it == soundDatas_.end()) return;

	if (it->second.pSourceVoice) {
		it->second.pSourceVoice->Stop();
	}
}