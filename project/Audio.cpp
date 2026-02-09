#include "Audio.h"
#include <cassert>
#include <fstream>

#pragma comment(lib, "xaudio2.lib")

void Audio::Initialize() {
	HRESULT result;

	// XAudio2エンジンのインスタンスを生成
	result = XAudio2Create(&xAudio2_, 0, XAUDIO2_DEFAULT_PROCESSOR);
	assert(SUCCEEDED(result));

	// マスターボイスを生成
	result = xAudio2_->CreateMasteringVoice(&masterVoice_);
	assert(SUCCEEDED(result));
}

void Audio::Finalize() {
	// XAudio2はComPtrで管理しているので自動解放されるが、
	// MasterVoiceは明示的にDestroyする場合もある（今回はデストラクタ任せでもOK）
	if (masterVoice_) {
		masterVoice_->DestroyVoice();
		masterVoice_ = nullptr;
	}
	xAudio2_.Reset();
	soundDatas_.clear();
}

uint32_t Audio::LoadWave(const std::string& filename) {
	// ファイルオープン
	std::ifstream file("Resources/" + filename, std::ios_base::binary);
	assert(file.is_open());

	// .wavファイル読み込み処理
	struct ChunkHeader {
		char id[4];   // チャンクID ("RIFF", "fmt ", "data" 等)
		int32_t size; // チャンクサイズ
	};
	struct RiffHeader {
		ChunkHeader chunk; // "RIFF"
		char type[4];      // "WAVE"
	};
	struct FormatChunk {
		ChunkHeader chunk; // "fmt "
		WAVEFORMATEX fmt;  // 波形フォーマット
	};

	// 1. RIFFチャンクの読み込み
	RiffHeader riff;
	file.read((char*)&riff, sizeof(riff));
	// ファイル先頭が "RIFF" で、タイプが "WAVE" かチェック
	if (strncmp(riff.chunk.id, "RIFF", 4) != 0 || strncmp(riff.type, "WAVE", 4) != 0) {
		assert(false); // ワイル形式が不正
	}

	// 2. チャンクを巡回して "fmt " と "data" を探す
	FormatChunk format = {};
	std::vector<BYTE> pBuffer;
	unsigned int bufferSize = 0;

	while (!file.eof()) {
		ChunkHeader header;
		file.read((char*)&header, sizeof(header));
		if (file.eof()) break;

		// "fmt " チャンクならフォーマットを読み込む
		if (strncmp(header.id, "fmt ", 4) == 0) {
			// チャンクサイズ分だけ読み込むが、WAVEFORMATEX構造体のサイズまで
			// (PCMなら16バイトや18バイトなど可変だが、WAVEFORMATEXで受け取る)
			// ここでは簡易的に読み込む
			assert(header.size <= sizeof(format.fmt));
			file.read((char*)&format.fmt, header.size);
		}
		// "data" チャンクなら波形データを読み込む
		else if (strncmp(header.id, "data", 4) == 0) {
			bufferSize = header.size;
			pBuffer.resize(bufferSize);
			file.read((char*)pBuffer.data(), bufferSize);
			// データ読み込み終わったらループを抜けても良いが、念のため最後まで
			break;
		} else {
			// 未知のチャンクはスキップ
			file.seekg(header.size, std::ios_base::cur);
		}
	}
	file.close();

	// サウンドデータを登録
	SoundData soundData = {};
	soundData.wfex = format.fmt;
	soundData.pBuffer = std::move(pBuffer);
	soundData.bufferSize = bufferSize;

	// ハンドルを発行して保存
	uint32_t handle = nextHandle_;
	soundDatas_[handle] = soundData;
	nextHandle_++;

	return handle;
}

void Audio::PlayWave(uint32_t soundHandle, bool loop, float volume) {
	HRESULT result;

	// データが存在するか確認
	if (soundDatas_.find(soundHandle) == soundDatas_.end()) {
		assert(false); // ハンドルが無効
		return;
	}

	SoundData& soundData = soundDatas_[soundHandle];

	// ソースボイスの生成
	IXAudio2SourceVoice* pSourceVoice = nullptr;
	result = xAudio2_->CreateSourceVoice(&pSourceVoice, &soundData.wfex);
	assert(SUCCEEDED(result));

	// バッファの設定
	XAUDIO2_BUFFER buffer = {};
	buffer.pAudioData = soundData.pBuffer.data();
	buffer.AudioBytes = soundData.bufferSize;
	buffer.Flags = XAUDIO2_END_OF_STREAM;

	// ループ設定
	if (loop) {
		buffer.LoopCount = XAUDIO2_LOOP_INFINITE;
	}

	// 波形データの送信
	result = pSourceVoice->SubmitSourceBuffer(&buffer);
	assert(SUCCEEDED(result));

	// ボリューム設定
	pSourceVoice->SetVolume(volume);

	// 再生開始
	result = pSourceVoice->Start();
	assert(SUCCEEDED(result));
}