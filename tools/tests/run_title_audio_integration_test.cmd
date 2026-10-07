@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\title_audio_obj mkdir generated\codex_checks\title_audio_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /Iproject\application /Iproject\engine tools\tests\title_audio_integration_test.cpp project\application\title\TitleAudioSystem.cpp project\engine\audio\Audio.cpp project\engine\base\Logger.cpp /Fo:generated\codex_checks\title_audio_obj\ /Fe:generated\codex_checks\title_audio_integration_test.exe /link ole32.lib
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\title_audio_integration_test.exe
