#pragma once

#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800  // DirectInputのバージョン指定
#include <dinput.h> //DirectInput

//入力
class Input {

	public:
	//初期化
	static void Initialize(HINSTANCE hInstance, HWND hwnd);
	//更新
	static void Update();


	

	

};

