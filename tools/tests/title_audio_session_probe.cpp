#define NOMINMAX
#include <Windows.h>
#include <mmdeviceapi.h>
#include <audiopolicy.h>
#include <endpointvolume.h>
#include <wrl.h>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#pragma comment(lib,"ole32.lib")
using Microsoft::WRL::ComPtr;

// Measures only the requested game process; does not record sound or inspect other sessions' peaks.
int main(int argc, char** argv) {
    if (argc != 3) return 2;
    const auto pid = static_cast<DWORD>(std::stoul(argv[1]));
    const bool expectSound = std::string(argv[2]) == "sound";
    const HRESULT initialized = CoInitializeEx(nullptr,COINIT_MULTITHREADED);
    if (FAILED(initialized)) return 3;
    int result = 1;
    {
        ComPtr<IMMDeviceEnumerator> devices;
        ComPtr<IMMDevice> device;
        ComPtr<IAudioSessionManager2> manager;
        ComPtr<IAudioSessionEnumerator> sessions;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&devices))) &&
            SUCCEEDED(devices->GetDefaultAudioEndpoint(eRender,eConsole,&device)) &&
            SUCCEEDED(device->Activate(__uuidof(IAudioSessionManager2),CLSCTX_ALL,nullptr,
                reinterpret_cast<void**>(manager.GetAddressOf()))) &&
            SUCCEEDED(manager->GetSessionEnumerator(&sessions))) {
            int count = 0;
            if (FAILED(sessions->GetCount(&count))) count = 0;
            std::cout << "render sessions=" << count << '\n';
            for (int i=0; i<count; ++i) {
                ComPtr<IAudioSessionControl> control;
                ComPtr<IAudioSessionControl2> identity;
                DWORD sessionPid = 0;
                if (FAILED(sessions->GetSession(i,&control)) || FAILED(control.As(&identity)) ||
                    FAILED(identity->GetProcessId(&sessionPid)) || sessionPid != pid) continue;
                ComPtr<IAudioMeterInformation> meter;
                const HRESULT meterResult = control.As(&meter);
                if (FAILED(meterResult)) {
                    std::cout << "game meter unavailable, HRESULT=" << std::hex << meterResult << '\n';
                    continue;
                }
                float maximum = 0;
                bool valid = true;
                for (int sample=0; sample<100; ++sample) {
                    float peak = 0;
                    if (FAILED(meter->GetPeakValue(&peak))) { valid = false; break; }
                    maximum = std::max(maximum,peak);
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
                std::cout << "game pid=" << pid << " max session peak=" << maximum << '\n';
                result = valid && (expectSound ? maximum > .00001f : maximum < .00001f) ? 0 : 1;
                break;
            }
        }
    }
    CoUninitialize();
    return result;
}
