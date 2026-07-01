#pragma once

#include "InputGamepadButton.h"
#include "InputKey.h"
#include "InputMouseButton.h"

#include <cstdint>
#include <unordered_map>
#include <vector>

class Input;

enum class InputBindingType {
	Keyboard,
	DirectInputKeyCode,
	MouseButton,
	GamepadButton,
};

struct InputBinding {
	InputBindingType type = InputBindingType::Keyboard;
	InputKey key = InputKey::Unknown;
	int keyCode = -1;
	InputMouseButton mouseButton = InputMouseButton::Left;
	InputGamepadButton gamepadButton = InputGamepadButton::A;
};

struct InputActionState {
	bool triggered = false;
	bool pressed = false;
	bool released = false;
	float holdTime = 0.0f;
};

class InputActionMap {
public:
	void BindKeyboard(uint32_t actionId, InputKey key);
	void BindKeyboard(uint32_t actionId, int keyCode);
	void BindMouseButton(uint32_t actionId, InputMouseButton button);
	void BindGamepadButton(uint32_t actionId, InputGamepadButton button);
	const std::vector<InputBinding>* FindBindings(uint32_t actionId) const;

private:
	friend class InputActionSystem;

	std::unordered_map<uint32_t, std::vector<InputBinding>> bindings_;
};

class InputActionSystem {
public:
	void Initialize(const InputActionMap& actionMap);
	void Update(Input& input, float deltaTime);

	bool IsTriggered(uint32_t actionId) const;
	bool IsPressed(uint32_t actionId) const;
	bool IsReleased(uint32_t actionId) const;
	float GetHoldTime(uint32_t actionId) const;

private:
	const InputActionState* FindState(uint32_t actionId) const;

	InputActionMap actionMap_;
	std::unordered_map<uint32_t, InputActionState> states_;
};
