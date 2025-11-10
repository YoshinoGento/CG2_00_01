#include "Logger.h"
#include <fstream>
#include <format>
#include <chrono>
#include <filesystem>
#include <errhandlingapi.h>

namespace Logger {

	void Log(const std::string& message) {

		// 誰も補足しなかった場合(Unhandled),補足する関数を登録
	// main関数はじまってすぐに登録するとよい
		SetUnhandledExceptionFilter(ExportDump);
		// ログのディレクトリを用意
		std::filesystem::create_directory("../generated/logs");
		// main関数の先頭02_04

		// 現在時刻を取得(UTC時刻)
		std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
		// ログファイルの名前にコンマ何秒はいらないので削って秒にする
		std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
			nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
		// 日本時間(PCの設定時間)に変換
		std::chrono::zoned_time loacalTime{ std::chrono::current_zone(), nowSeconds };
		// formatを使って年月日_時分秒の文字列に変換
		std::string dateString = std::format("{:%Y%m%d_%H%M%S}", loacalTime);
		// 時刻を使ってファイル名を決定
		std::string logFilePath = std::string("../generated/logs/") + dateString + ".log";
		// ファイルを作って書き込み準備
		std::ofstream logStream(logFilePath);

	}

}