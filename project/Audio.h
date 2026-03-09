#pragma once
#include <wrl.h>
#include <xaudio2.h>
#include <vector>
#include <string>
#include <map>

/**
 * Audioクラス
 * WAV, MP3, AAC などの読み込み、再生、停止を管理します。
 * 内部で Media Foundation を使用しているため、圧縮音源も再生可能です。
 */
class Audio {
public:
	// 音声ファイル1つ分のデータを保持する構造体
	struct SoundData {
		WAVEFORMATEX wfex;              // 波形フォーマット（サンプリングレート等）
		std::vector<byte> pBuffer;      // 音声データ本体（デコード済みのPCM）
		uint32_t bufferSize;            // データのバイト数
		IXAudio2SourceVoice* pSourceVoice; // この音専用の再生機（ボイス）
	};

	// --- 基本機能 ---

	// 初期化：エンジンの起動時に一度だけ呼ぶ
	void Initialize();
	// 終了処理：エンジンの終了時に一度だけ呼ぶ
	void Finalize();

	/**
	 * 音声ファイルの読み込み（一度読み込めばメモリに保持されます）
	 * @param filename ファイルパス（例："Resources/bgm.mp3"）
	 * @return 管理用のハンドル（ID）
	 */
	uint32_t LoadAudio(const std::string& filename);

	/**
	 * 音声の再生
	 * @param handle LoadAudioで取得したハンドル
	 * @param loop trueでループ再生、falseで1回再生
	 */
	void PlayWave(uint32_t handle, bool loop = false);

	/**
	 * 音声の停止
	 * @param handle 止めたい音のハンドル
	 */
	void StopWave(uint32_t handle);

private:
	Microsoft::WRL::ComPtr<IXAudio2> xAudio2_; // XAudio2本体
	IXAudio2MasteringVoice* masterVoice_ = nullptr; // 最終的な音の出口

	std::map<uint32_t, SoundData> soundDatas_; // 読み込んだデータの管理用
	uint32_t nextHandle_ = 0; // 次に発行するハンドルの番号
};