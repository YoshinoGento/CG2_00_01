#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

template <class Snapshot>
class SnapshotTimeline final {
public:
	void Initialize(std::size_t capacity, const Snapshot& initialSnapshot) {
		capacity_ = (std::max)(capacity, std::size_t{ 2 });
		slots_.assign(capacity_, initialSnapshot);
		oldestIndex_ = 0;
		count_ = 1;
		cursor_ = 0;
		initialized_ = true;
	}

	void Clear() noexcept {
		slots_.clear();
		capacity_ = 0;
		oldestIndex_ = 0;
		count_ = 0;
		cursor_ = 0;
		initialized_ = false;
	}

	bool Record(const Snapshot& snapshot) {
		if (!initialized_) {
			return false;
		}

		// Recording after rewind creates a new branch and invalidates the old future.
		if (cursor_ + 1 < count_) {
			count_ = cursor_ + 1;
		}
		if (count_ < capacity_) {
			const std::size_t writeIndex = PhysicalIndex(count_);
			slots_[writeIndex] = snapshot;
			++count_;
			cursor_ = count_ - 1;
			return true;
		}

		oldestIndex_ = (oldestIndex_ + 1) % capacity_;
		const std::size_t writeIndex = PhysicalIndex(count_ - 1);
		slots_[writeIndex] = snapshot;
		cursor_ = count_ - 1;
		return true;
	}

	bool StepBackward(Snapshot& output) {
		if (!CanStepBackward()) {
			return false;
		}
		--cursor_;
		output = slots_[PhysicalIndex(cursor_)];
		return true;
	}

	bool StepForward(Snapshot& output) {
		if (!CanStepForward()) {
			return false;
		}
		++cursor_;
		output = slots_[PhysicalIndex(cursor_)];
		return true;
	}

	[[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }
	[[nodiscard]] bool CanStepBackward() const noexcept { return initialized_ && cursor_ > 0; }
	[[nodiscard]] bool CanStepForward() const noexcept { return initialized_ && cursor_ + 1 < count_; }
	[[nodiscard]] std::size_t GetSnapshotCount() const noexcept { return count_; }
	[[nodiscard]] std::size_t GetCursor() const noexcept { return cursor_; }
	[[nodiscard]] std::size_t GetCapacity() const noexcept { return capacity_; }

private:
	[[nodiscard]] std::size_t PhysicalIndex(std::size_t logicalIndex) const noexcept {
		return (oldestIndex_ + logicalIndex) % capacity_;
	}

	std::vector<Snapshot> slots_;
	std::size_t capacity_ = 0;
	std::size_t oldestIndex_ = 0;
	std::size_t count_ = 0;
	std::size_t cursor_ = 0;
	bool initialized_ = false;
};
