#include "Logger.h"
#include <Windows.h>

namespace Logger {

    void Log(const std::string& message) {
        // Visual Studio の出力ウィンドウに表示
        OutputDebugStringA(message.c_str());
        OutputDebugStringA("\n");
    }

}
