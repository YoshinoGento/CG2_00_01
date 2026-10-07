@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\title_settings_obj mkdir generated\codex_checks\title_settings_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /Iproject /Iproject\application /Iproject\engine tools\tests\title_audio_settings_test.cpp project\application\title\TitleAudioSettingsSystem.cpp project\engine\io\JsonFile.cpp project\engine\base\Logger.cpp /Fo:generated\codex_checks\title_settings_obj\ /Fe:generated\codex_checks\title_audio_settings_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\title_audio_settings_test.exe
