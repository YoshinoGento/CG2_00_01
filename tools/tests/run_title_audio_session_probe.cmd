@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\title_audio_probe_obj mkdir generated\codex_checks\title_audio_probe_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX tools\tests\title_audio_session_probe.cpp /Fo:generated\codex_checks\title_audio_probe_obj\ /Fe:generated\codex_checks\title_audio_session_probe.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\title_audio_session_probe.exe %1 %2
