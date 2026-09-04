#pragma once
#include <Windows.h>
#define DIRECTINPUT_VERSION 0x0800  // DirectInputのバージョン指定
#include <dinput.h> //DirectInput
#include <wrl.h>
#include "base/WinApp.h"
#include "io/InputGamepadButton.h"
#include "io/InputKey.h"
#include "io/InputMouseButton.h"
#include "math/Struct.h"

using namespace Microsoft::WRL;


//入力
class Input {

public:
	//namespace省略
	template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;

	//初期化
	void Initialize(WinApp * winApp);
	//更新
	void Update();

	/// <summary>
	/// キーを押下をチェック
	/// </summary>
	/// <param name="keyNumber">キー番号</param>
	/// <returns>押されているか</returns>
	bool PushKey(BYTE keyNumber) const; //キーが押された瞬間
	bool PushKey(InputKey key) const;


	/// <summary>
	/// キーのトリガーをチェック
	/// </summary>
	/// <param name="keyNumber">キー番号</param>
	/// <returns>トリガーか</returns>
	bool TriggerKey(BYTE keyNumber) const; 
	bool TriggerKey(InputKey key) const;
	bool ReleaseKey(BYTE keyNumber) const;
	bool ReleaseKey(InputKey key) const;

	bool PushMouseButton(InputMouseButton button) const;
	bool TriggerMouseButton(InputMouseButton button) const;
	bool ReleaseMouseButton(InputMouseButton button) const;
	Vector2 GetMousePosition() const;
	Vector2 GetMouseDelta() const;
	float GetMouseWheelDelta() const;
	bool IsGamepadConnected() const;
	bool PushGamepadButton(InputGamepadButton button) const;
	bool TriggerGamepadButton(InputGamepadButton button) const;
	bool ReleaseGamepadButton(InputGamepadButton button) const;
	Vector2 GetLeftStick() const;
	Vector2 GetRightStick() const;
	float GetLeftTrigger() const;
	float GetRightTrigger() const;

private:
	bool UpdateKeyboardState();
	bool UpdateMouseState();
	bool UpdateGamepadState();
	void UpdateMousePosition();
	void ClearKeyboardState();
	void ClearMouseState();
	void ClearGamepadState();

	//キーボードデバイス
	ComPtr<IDirectInputDevice8> keyboard;
	ComPtr<IDirectInputDevice8> mouse;
	
	//DirectInputのインスタンスの生成
	ComPtr<IDirectInput8> directInput = nullptr;

	//全キーの状態
	BYTE keyPre[256] = {};

	BYTE key[256] = {};

	static constexpr int kMouseButtonCount = 5;
	BYTE mouseButtonPre[kMouseButtonCount] = {};
	BYTE mouseButton[kMouseButtonCount] = {};
	Vector2 mousePosition_ = { 0.0f, 0.0f };
	Vector2 mouseDelta_ = { 0.0f, 0.0f };
	float mouseWheelDelta_ = 0.0f;
	bool isGamepadConnected_ = false;
	unsigned short gamepadButtonsPre_ = 0;
	unsigned short gamepadButtons_ = 0;
	Vector2 leftStick_ = { 0.0f, 0.0f };
	Vector2 rightStick_ = { 0.0f, 0.0f };
	float leftTrigger_ = 0.0f;
	float rightTrigger_ = 0.0f;
	
	//WindowsAPI
	WinApp* winApp_ = nullptr;

};

