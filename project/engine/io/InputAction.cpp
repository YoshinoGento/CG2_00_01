#include "InputAction.h"

#include "Input.h"

namespace {
	bool IsKeyboardKeyCodeValid(int keyCode)
	{
		constexpr int kMinKeyCode = 0;
		constexpr int kMaxKeyCode = 255;
		return keyCode >= kMinKeyCode && keyCode <= kMaxKeyCode;
	}
}

void InputActionMap::BindKeyboard(uint32_t actionId, int keyCode)
{
	bindings_[actionId].push_back(InputBinding{ keyCode });
}

const std::vector<InputBinding>* InputActionMap::FindBindings(uint32_t actionId) const
{
	const auto it = bindings_.find(actionId);
	if (it == bindings_.end()) {
		return nullptr;
	}

	return &it->second;
}

void InputActionSystem::Initialize(const InputActionMap& actionMap)
{
	actionMap_ = actionMap;
	states_.clear();

	for (const auto& [actionId, bindings] : actionMap_.bindings_) {
		(void)bindings;
		states_.emplace(actionId, InputActionState{});
	}
}

void InputActionSystem::Update(Input& input, float deltaTime)
{
	for (const auto& [actionId, bindings] : actionMap_.bindings_) {
		InputActionState& state = states_[actionId];
		const bool wasPressed = state.pressed;

		bool isTriggered = false;
		bool isPressed = false;

		for (const InputBinding& binding : bindings) {
			if (!IsKeyboardKeyCodeValid(binding.keyCode)) {
				continue;
			}

			const BYTE keyCode = static_cast<BYTE>(binding.keyCode);
			isTriggered = input.TriggerKey(keyCode) || isTriggered;
			isPressed = input.PushKey(keyCode) || isPressed;
		}

		state.triggered = isTriggered;
		state.pressed = isPressed;
		state.released = wasPressed && !isPressed;
		state.holdTime = isPressed ? state.holdTime + deltaTime : 0.0f;
	}
}

bool InputActionSystem::IsTriggered(uint32_t actionId) const
{
	const InputActionState* state = FindState(actionId);
	return state != nullptr && state->triggered;
}

bool InputActionSystem::IsPressed(uint32_t actionId) const
{
	const InputActionState* state = FindState(actionId);
	return state != nullptr && state->pressed;
}

bool InputActionSystem::IsReleased(uint32_t actionId) const
{
	const InputActionState* state = FindState(actionId);
	return state != nullptr && state->released;
}

float InputActionSystem::GetHoldTime(uint32_t actionId) const
{
	const InputActionState* state = FindState(actionId);
	return state != nullptr ? state->holdTime : 0.0f;
}

const InputActionState* InputActionSystem::FindState(uint32_t actionId) const
{
	const auto it = states_.find(actionId);
	if (it == states_.end()) {
		return nullptr;
	}

	return &it->second;
}
