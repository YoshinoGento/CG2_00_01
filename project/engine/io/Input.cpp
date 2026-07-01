#include "Input.h"

#include <assert.h>
#include <cassert>
#include <cstring>
#include <Xinput.h>

#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "xinput.lib")

namespace {
	constexpr int kMaxAcquireAttempts = 8;

	bool IsRecoverableInputError(HRESULT result)
	{
		return result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED;
	}

	bool TryAcquireDevice(IDirectInputDevice8* device)
	{
		if (!device) {
			return false;
		}

		for (int attempt = 0; attempt < kMaxAcquireAttempts; ++attempt) {
			const HRESULT result = device->Acquire();
			if (SUCCEEDED(result)) {
				return true;
			}
			if (!IsRecoverableInputError(result)) {
				return false;
			}
		}

		return false;
	}

	int GetMouseButtonIndex(InputMouseButton button)
	{
		switch (button) {
		case InputMouseButton::Left:
			return 0;
		case InputMouseButton::Right:
			return 1;
		case InputMouseButton::Middle:
			return 2;
		case InputMouseButton::X1:
			return 3;
		case InputMouseButton::X2:
			return 4;
		default:
			return -1;
		}
	}

	unsigned short ToXInputGamepadButton(InputGamepadButton button)
	{
		switch (button) {
		case InputGamepadButton::A:
			return XINPUT_GAMEPAD_A;
		case InputGamepadButton::B:
			return XINPUT_GAMEPAD_B;
		case InputGamepadButton::X:
			return XINPUT_GAMEPAD_X;
		case InputGamepadButton::Y:
			return XINPUT_GAMEPAD_Y;
		case InputGamepadButton::LeftShoulder:
			return XINPUT_GAMEPAD_LEFT_SHOULDER;
		case InputGamepadButton::RightShoulder:
			return XINPUT_GAMEPAD_RIGHT_SHOULDER;
		case InputGamepadButton::Back:
			return XINPUT_GAMEPAD_BACK;
		case InputGamepadButton::Start:
			return XINPUT_GAMEPAD_START;
		case InputGamepadButton::LeftThumb:
			return XINPUT_GAMEPAD_LEFT_THUMB;
		case InputGamepadButton::RightThumb:
			return XINPUT_GAMEPAD_RIGHT_THUMB;
		case InputGamepadButton::DPadUp:
			return XINPUT_GAMEPAD_DPAD_UP;
		case InputGamepadButton::DPadDown:
			return XINPUT_GAMEPAD_DPAD_DOWN;
		case InputGamepadButton::DPadLeft:
			return XINPUT_GAMEPAD_DPAD_LEFT;
		case InputGamepadButton::DPadRight:
			return XINPUT_GAMEPAD_DPAD_RIGHT;
		default:
			return 0;
		}
	}

	float NormalizeThumbAxis(short value, short deadZone)
	{
		const int axisValue = static_cast<int>(value);
		const int absValue = axisValue < 0 ? -axisValue : axisValue;
		if (absValue <= deadZone) {
			return 0.0f;
		}

		const float maxValue = axisValue < 0 ? 32768.0f : 32767.0f;
		return static_cast<float>(axisValue) / maxValue;
	}

	float NormalizeTrigger(unsigned char value)
	{
		if (value <= XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
			return 0.0f;
		}

		constexpr float kMaxTriggerValue = 255.0f;
		constexpr float kTriggerThreshold = static_cast<float>(XINPUT_GAMEPAD_TRIGGER_THRESHOLD);
		return (static_cast<float>(value) - kTriggerThreshold) / (kMaxTriggerValue - kTriggerThreshold);
	}
}

void Input::Initialize(WinApp* winApp)
{
	this->winApp_ = winApp;

	HRESULT result;
	result = DirectInput8Create(winApp->GetHInstance(), DIRECTINPUT_VERSION, IID_IDirectInput8, (void**)&directInput, nullptr);
	assert(SUCCEEDED(result));

	result = directInput->CreateDevice(GUID_SysKeyboard, &keyboard, NULL);
	assert(SUCCEEDED(result));

	result = keyboard->SetDataFormat(&c_dfDIKeyboard);
	assert(SUCCEEDED(result));

	result = keyboard->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE | DISCL_NOWINKEY);
	assert(SUCCEEDED(result));

	result = directInput->CreateDevice(GUID_SysMouse, &mouse, NULL);
	assert(SUCCEEDED(result));

	result = mouse->SetDataFormat(&c_dfDIMouse2);
	assert(SUCCEEDED(result));

	result = mouse->SetCooperativeLevel(winApp->GetHwnd(), DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);
	assert(SUCCEEDED(result));
}

void Input::Update()
{
	memcpy(keyPre, key, sizeof(key));
	if (!UpdateKeyboardState()) {
		ClearKeyboardState();
	}

	memcpy(mouseButtonPre, mouseButton, sizeof(mouseButton));
	if (!UpdateMouseState()) {
		ClearMouseState();
	}

	gamepadButtonsPre_ = gamepadButtons_;
	if (!UpdateGamepadState()) {
		ClearGamepadState();
	}

	UpdateMousePosition();
}

bool Input::UpdateKeyboardState()
{
	if (!keyboard) {
		return false;
	}

	TryAcquireDevice(keyboard.Get());

	HRESULT result = keyboard->GetDeviceState(sizeof(key), key);
	if (FAILED(result) && IsRecoverableInputError(result)) {
		if (TryAcquireDevice(keyboard.Get())) {
			result = keyboard->GetDeviceState(sizeof(key), key);
		}
	}

	return SUCCEEDED(result);
}

bool Input::UpdateMouseState()
{
	if (!mouse) {
		return false;
	}

	mouseDelta_ = { 0.0f, 0.0f };
	mouseWheelDelta_ = 0.0f;

	DIMOUSESTATE2 mouseState{};
	TryAcquireDevice(mouse.Get());

	HRESULT result = mouse->GetDeviceState(sizeof(mouseState), &mouseState);
	if (FAILED(result) && IsRecoverableInputError(result)) {
		if (TryAcquireDevice(mouse.Get())) {
			result = mouse->GetDeviceState(sizeof(mouseState), &mouseState);
		}
	}

	if (FAILED(result)) {
		return false;
	}

	for (int i = 0; i < kMouseButtonCount; ++i) {
		mouseButton[i] = mouseState.rgbButtons[i];
	}
	mouseDelta_ = {
		static_cast<float>(mouseState.lX),
		static_cast<float>(mouseState.lY)
	};
	mouseWheelDelta_ = static_cast<float>(mouseState.lZ);
	return true;
}

bool Input::UpdateGamepadState()
{
	XINPUT_STATE state{};
	const DWORD result = XInputGetState(0, &state);
	if (result != ERROR_SUCCESS) {
		return false;
	}

	isGamepadConnected_ = true;
	gamepadButtons_ = state.Gamepad.wButtons;
	leftStick_ = {
		NormalizeThumbAxis(state.Gamepad.sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE),
		NormalizeThumbAxis(state.Gamepad.sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE)
	};
	rightStick_ = {
		NormalizeThumbAxis(state.Gamepad.sThumbRX, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE),
		NormalizeThumbAxis(state.Gamepad.sThumbRY, XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE)
	};
	leftTrigger_ = NormalizeTrigger(state.Gamepad.bLeftTrigger);
	rightTrigger_ = NormalizeTrigger(state.Gamepad.bRightTrigger);
	return true;
}

void Input::UpdateMousePosition()
{
	POINT cursorPosition{};
	if (GetCursorPos(&cursorPosition)) {
		ScreenToClient(winApp_->GetHwnd(), &cursorPosition);
		mousePosition_ = {
			static_cast<float>(cursorPosition.x),
			static_cast<float>(cursorPosition.y)
		};
	}
}

void Input::ClearKeyboardState()
{
	memset(keyPre, 0, sizeof(keyPre));
	memset(key, 0, sizeof(key));
}

void Input::ClearMouseState()
{
	memset(mouseButtonPre, 0, sizeof(mouseButtonPre));
	memset(mouseButton, 0, sizeof(mouseButton));
	mouseDelta_ = { 0.0f, 0.0f };
	mouseWheelDelta_ = 0.0f;
}

void Input::ClearGamepadState()
{
	isGamepadConnected_ = false;
	gamepadButtonsPre_ = 0;
	gamepadButtons_ = 0;
	leftStick_ = { 0.0f, 0.0f };
	rightStick_ = { 0.0f, 0.0f };
	leftTrigger_ = 0.0f;
	rightTrigger_ = 0.0f;
}

bool Input::PushKey(BYTE keyNumber)
{
	if (key[keyNumber]) {
		return true;
	}

	return false;
}

bool Input::TriggerKey(BYTE keyNumber)
{
	if (key[keyNumber] && !keyPre[keyNumber]) {
		return true;
	}

	return false;
}

bool Input::PushMouseButton(InputMouseButton button) const
{
	const int index = GetMouseButtonIndex(button);
	return index >= 0 && index < kMouseButtonCount && mouseButton[index] != 0;
}

bool Input::TriggerMouseButton(InputMouseButton button) const
{
	const int index = GetMouseButtonIndex(button);
	return index >= 0 && index < kMouseButtonCount && mouseButton[index] != 0 && mouseButtonPre[index] == 0;
}

bool Input::ReleaseMouseButton(InputMouseButton button) const
{
	const int index = GetMouseButtonIndex(button);
	return index >= 0 && index < kMouseButtonCount && mouseButton[index] == 0 && mouseButtonPre[index] != 0;
}

Vector2 Input::GetMousePosition() const
{
	return mousePosition_;
}

Vector2 Input::GetMouseDelta() const
{
	return mouseDelta_;
}

float Input::GetMouseWheelDelta() const
{
	return mouseWheelDelta_;
}

bool Input::IsGamepadConnected() const
{
	return isGamepadConnected_;
}

bool Input::PushGamepadButton(InputGamepadButton button) const
{
	const unsigned short buttonMask = ToXInputGamepadButton(button);
	return buttonMask != 0 && (gamepadButtons_ & buttonMask) != 0;
}

bool Input::TriggerGamepadButton(InputGamepadButton button) const
{
	const unsigned short buttonMask = ToXInputGamepadButton(button);
	return buttonMask != 0 && (gamepadButtons_ & buttonMask) != 0 && (gamepadButtonsPre_ & buttonMask) == 0;
}

bool Input::ReleaseGamepadButton(InputGamepadButton button) const
{
	const unsigned short buttonMask = ToXInputGamepadButton(button);
	return buttonMask != 0 && (gamepadButtons_ & buttonMask) == 0 && (gamepadButtonsPre_ & buttonMask) != 0;
}

Vector2 Input::GetLeftStick() const
{
	return leftStick_;
}

Vector2 Input::GetRightStick() const
{
	return rightStick_;
}

float Input::GetLeftTrigger() const
{
	return leftTrigger_;
}

float Input::GetRightTrigger() const
{
	return rightTrigger_;
}
