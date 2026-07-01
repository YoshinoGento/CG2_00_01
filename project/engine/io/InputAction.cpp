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

int ToDirectInputKey(InputKey key) noexcept
{
	switch (key) {
	case InputKey::A:
		return DIK_A;
	case InputKey::B:
		return DIK_B;
	case InputKey::C:
		return DIK_C;
	case InputKey::D:
		return DIK_D;
	case InputKey::E:
		return DIK_E;
	case InputKey::F:
		return DIK_F;
	case InputKey::G:
		return DIK_G;
	case InputKey::H:
		return DIK_H;
	case InputKey::I:
		return DIK_I;
	case InputKey::J:
		return DIK_J;
	case InputKey::K:
		return DIK_K;
	case InputKey::L:
		return DIK_L;
	case InputKey::M:
		return DIK_M;
	case InputKey::N:
		return DIK_N;
	case InputKey::O:
		return DIK_O;
	case InputKey::P:
		return DIK_P;
	case InputKey::Q:
		return DIK_Q;
	case InputKey::R:
		return DIK_R;
	case InputKey::S:
		return DIK_S;
	case InputKey::T:
		return DIK_T;
	case InputKey::U:
		return DIK_U;
	case InputKey::V:
		return DIK_V;
	case InputKey::W:
		return DIK_W;
	case InputKey::X:
		return DIK_X;
	case InputKey::Y:
		return DIK_Y;
	case InputKey::Z:
		return DIK_Z;
	case InputKey::Num0:
		return DIK_0;
	case InputKey::Num1:
		return DIK_1;
	case InputKey::Num2:
		return DIK_2;
	case InputKey::Num3:
		return DIK_3;
	case InputKey::Num4:
		return DIK_4;
	case InputKey::Num5:
		return DIK_5;
	case InputKey::Num6:
		return DIK_6;
	case InputKey::Num7:
		return DIK_7;
	case InputKey::Num8:
		return DIK_8;
	case InputKey::Num9:
		return DIK_9;
	case InputKey::Escape:
		return DIK_ESCAPE;
	case InputKey::Space:
		return DIK_SPACE;
	case InputKey::Enter:
		return DIK_RETURN;
	case InputKey::Tab:
		return DIK_TAB;
	case InputKey::LeftShift:
		return DIK_LSHIFT;
	case InputKey::RightShift:
		return DIK_RSHIFT;
	case InputKey::Comma:
		return DIK_COMMA;
	case InputKey::Period:
		return DIK_PERIOD;
	case InputKey::Unknown:
	default:
		return -1;
	}
}

void InputActionMap::BindKeyboard(uint32_t actionId, InputKey key)
{
	InputBinding binding{};
	binding.type = InputBindingType::Keyboard;
	binding.key = key;
	bindings_[actionId].push_back(binding);
}

void InputActionMap::BindKeyboard(uint32_t actionId, int keyCode)
{
	InputBinding binding{};
	binding.type = InputBindingType::DirectInputKeyCode;
	binding.keyCode = keyCode;
	bindings_[actionId].push_back(binding);
}

void InputActionMap::BindMouseButton(uint32_t actionId, InputMouseButton button)
{
	InputBinding binding{};
	binding.type = InputBindingType::MouseButton;
	binding.mouseButton = button;
	bindings_[actionId].push_back(binding);
}

void InputActionMap::BindGamepadButton(uint32_t actionId, InputGamepadButton button)
{
	InputBinding binding{};
	binding.type = InputBindingType::GamepadButton;
	binding.gamepadButton = button;
	bindings_[actionId].push_back(binding);
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
			if (binding.type == InputBindingType::MouseButton) {
				isTriggered = input.TriggerMouseButton(binding.mouseButton) || isTriggered;
				isPressed = input.PushMouseButton(binding.mouseButton) || isPressed;
			}
			else if (binding.type == InputBindingType::GamepadButton) {
				isTriggered = input.TriggerGamepadButton(binding.gamepadButton) || isTriggered;
				isPressed = input.PushGamepadButton(binding.gamepadButton) || isPressed;
			}
			else {
				const int keyCodeValue = binding.type == InputBindingType::DirectInputKeyCode
					? binding.keyCode
					: ToDirectInputKey(binding.key);
				if (!IsKeyboardKeyCodeValid(keyCodeValue)) {
					continue;
				}

				const BYTE keyCode = static_cast<BYTE>(keyCodeValue);
				isTriggered = input.TriggerKey(keyCode) || isTriggered;
				isPressed = input.PushKey(keyCode) || isPressed;
			}
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
