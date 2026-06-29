#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

class Input;

struct InputBinding {
	int keyCode = 0;
};

struct InputActionState {
	bool triggered = false;
	bool pressed = false;
	bool released = false;
	float holdTime = 0.0f;
};

class InputActionMap {
public:
	void BindKeyboard(uint32_t actionId, int keyCode);
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
