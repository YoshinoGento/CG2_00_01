#pragma once

#include "debug/CG4EvaluationTypes.h"

class CG4EvaluationWindow final {
public:
	// true: CG4評価用UI、false: 従来デバッグUI。
	[[nodiscard]] std::optional<bool> DrawModeBar(bool evaluationMode);
	[[nodiscard]] CG4EvaluationActions Draw(const CG4EvaluationViewData& viewData);
};
