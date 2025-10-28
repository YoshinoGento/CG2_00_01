#pragma once

#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800  // DirectInputのバージョン指定
#include <dinput.h> //DirectInput
#include <wrl.h>

using namespace Microsoft::WRL;


//入力
class Input {

public:
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	//初期化
	void Initialize(HINSTANCE hInstance, HWND hwnd);
	//更新
	void Update();

	//namespace省略

private:
	//キーボードデバイス
	ComPtr<IDirectInputDevice8> keyboard;
	

	

};

