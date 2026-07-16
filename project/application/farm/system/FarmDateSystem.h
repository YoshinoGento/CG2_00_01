#pragma once

class FarmDateSystem {
public:
	struct Snapshot {
		int day = 1;
		float elapsedSecondsInDay = 0.0f;
		float timeScale = 1.0f;
	};

	void Initialize();

	void Update(float deltaTime);

	void SetTimeScale(float timeScale);
	void CycleTimeScale();
	void AdvanceOneDay();

	int GetDay() const { return day_; }
	float GetElapsedSecondsInDay() const { return elapsedSecondsInDay_; }
	float GetDayLengthSeconds() const { return dayLengthSeconds_; }
	float GetTimeScale() const { return timeScale_; }
	float GetDayProgress() const;
	[[nodiscard]] Snapshot CaptureSnapshot() const noexcept;
	bool RestoreSnapshot(const Snapshot& snapshot);

private:
	int day_ = 1;
	float elapsedSecondsInDay_ = 0.0f;
	float dayLengthSeconds_ = 60.0f;
	float timeScale_ = 1.0f;
};
