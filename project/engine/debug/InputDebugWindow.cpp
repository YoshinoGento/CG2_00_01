#include "debug/InputDebugWindow.h"

#include "externals/imgui/imgui.h"
#include "io/Input.h"
#include "io/InputKey.h"

namespace {
	struct KeyboardDebugItem {
		const char* label;
		InputKey key;
	};

	struct MouseDebugItem {
		const char* label;
		InputMouseButton button;
	};

	struct GamepadDebugItem {
		const char* label;
		InputGamepadButton button;
	};

	void DrawStateRow(const char* label, bool pressed, bool triggered, bool released)
	{
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(label);
		ImGui::TableSetColumnIndex(1);
		ImGui::TextUnformatted(pressed ? "true" : "false");
		ImGui::TableSetColumnIndex(2);
		ImGui::TextUnformatted(triggered ? "true" : "false");
		ImGui::TableSetColumnIndex(3);
		ImGui::TextUnformatted(released ? "true" : "false");
	}

	void DrawTableHeader()
	{
		ImGui::TableSetupColumn("Input");
		ImGui::TableSetupColumn("Pressed");
		ImGui::TableSetupColumn("Triggered");
		ImGui::TableSetupColumn("Released");
		ImGui::TableHeadersRow();
	}

	void DrawKeyboardSection(const Input& input)
	{
		if (!ImGui::CollapsingHeader("Keyboard", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		const KeyboardDebugItem items[] = {
			{ "InputKey::E", InputKey::E },
			{ "InputKey::Q", InputKey::Q },
			{ "InputKey::R", InputKey::R },
			{ "InputKey::Space", InputKey::Space },
			{ "InputKey::Escape", InputKey::Escape },
			{ "InputKey::Tab", InputKey::Tab },
		};

		if (ImGui::BeginTable("InputDebugKeyboardTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			DrawTableHeader();
			for (const KeyboardDebugItem& item : items) {
				DrawStateRow(item.label, input.PushKey(item.key), input.TriggerKey(item.key), input.ReleaseKey(item.key));
			}
			ImGui::EndTable();
		}
	}

	void DrawMouseSection(const Input& input)
	{
		if (!ImGui::CollapsingHeader("Mouse", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		const MouseDebugItem items[] = {
			{ "InputMouseButton::Left", InputMouseButton::Left },
			{ "InputMouseButton::Right", InputMouseButton::Right },
			{ "InputMouseButton::Middle", InputMouseButton::Middle },
		};

		if (ImGui::BeginTable("InputDebugMouseTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			DrawTableHeader();
			for (const MouseDebugItem& item : items) {
				DrawStateRow(
					item.label,
					input.PushMouseButton(item.button),
					input.TriggerMouseButton(item.button),
					input.ReleaseMouseButton(item.button));
			}
			ImGui::EndTable();
		}

		const Vector2 mousePosition = input.GetMousePosition();
		const Vector2 mouseDelta = input.GetMouseDelta();
		ImGui::Text("Mouse Position: %.1f, %.1f", mousePosition.x, mousePosition.y);
		ImGui::Text("Mouse Delta   : %.1f, %.1f", mouseDelta.x, mouseDelta.y);
		ImGui::Text("Mouse Wheel Delta: %.1f", input.GetMouseWheelDelta());
	}

	void DrawGamepadSection(const Input& input)
	{
		if (!ImGui::CollapsingHeader("Gamepad", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		ImGui::Text("Connected: %s", input.IsGamepadConnected() ? "true" : "false");

		const GamepadDebugItem items[] = {
			{ "InputGamepadButton::A", InputGamepadButton::A },
			{ "InputGamepadButton::B", InputGamepadButton::B },
			{ "InputGamepadButton::X", InputGamepadButton::X },
			{ "InputGamepadButton::Y", InputGamepadButton::Y },
			{ "InputGamepadButton::LeftShoulder", InputGamepadButton::LeftShoulder },
			{ "InputGamepadButton::RightShoulder", InputGamepadButton::RightShoulder },
		};

		if (ImGui::BeginTable("InputDebugGamepadTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
			DrawTableHeader();
			for (const GamepadDebugItem& item : items) {
				DrawStateRow(
					item.label,
					input.PushGamepadButton(item.button),
					input.TriggerGamepadButton(item.button),
					input.ReleaseGamepadButton(item.button));
			}
			ImGui::EndTable();
		}

		const Vector2 leftStick = input.GetLeftStick();
		const Vector2 rightStick = input.GetRightStick();
		ImGui::Text("Left Stick : %.3f, %.3f", leftStick.x, leftStick.y);
		ImGui::Text("Right Stick: %.3f, %.3f", rightStick.x, rightStick.y);
		ImGui::Text("Left Trigger : %.3f", input.GetLeftTrigger());
		ImGui::Text("Right Trigger: %.3f", input.GetRightTrigger());
	}
}

void InputDebugWindow::Draw(const Input& input)
{
	if (ImGui::Begin("Input Debug")) {
		DrawKeyboardSection(input);
		DrawMouseSection(input);
		DrawGamepadSection(input);
	}
	ImGui::End();
}
