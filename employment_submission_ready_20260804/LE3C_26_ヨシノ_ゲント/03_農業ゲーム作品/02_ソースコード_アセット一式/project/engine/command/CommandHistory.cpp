#include "command/CommandHistory.h"

#include <algorithm>

CommandHistory::CommandHistory(std::size_t capacity)
	: capacity_((std::max)(capacity, std::size_t{ 1 })) {
	commands_.reserve(capacity_);
}

bool CommandHistory::Execute(std::unique_ptr<IUndoableCommand> command) {
	if (!command || !command->Execute()) {
		return false;
	}
	if (cursor_ < commands_.size()) {
		commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());
	}
	if (commands_.size() == capacity_) {
		commands_.erase(commands_.begin());
		if (cursor_ > 0) {
			--cursor_;
		}
	}
	commands_.push_back(std::move(command));
	cursor_ = commands_.size();
	return true;
}

bool CommandHistory::Undo() {
	if (!CanUndo()) {
		return false;
	}
	IUndoableCommand* command = commands_[cursor_ - 1].get();
	if (command == nullptr || !command->Undo()) {
		return false;
	}
	--cursor_;
	return true;
}

bool CommandHistory::Redo() {
	if (!CanRedo()) {
		return false;
	}
	IUndoableCommand* command = commands_[cursor_].get();
	if (command == nullptr || !command->Execute()) {
		return false;
	}
	++cursor_;
	return true;
}

void CommandHistory::Clear() noexcept {
	commands_.clear();
	cursor_ = 0;
}

std::string_view CommandHistory::GetUndoName() const noexcept {
	return CanUndo() ? commands_[cursor_ - 1]->GetName() : std::string_view{};
}

std::string_view CommandHistory::GetRedoName() const noexcept {
	return CanRedo() ? commands_[cursor_]->GetName() : std::string_view{};
}
