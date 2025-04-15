#include <windows.h>

//Windowsアプリでのエントリーポイント(main関数)
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	//出力ウィンドウ絵の文字出力
	OutputDebugStringA("Hello,DirectX!\n");

	return 0;
}