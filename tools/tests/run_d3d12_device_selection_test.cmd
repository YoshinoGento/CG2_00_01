@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\device_obj mkdir generated\codex_checks\device_obj
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /Iproject\engine /Fo:generated\codex_checks\device_obj\ tools\tests\d3d12_device_selection_test.cpp /Fe:generated\codex_checks\d3d12_device_selection_test.exe /link d3d12.lib dxgi.lib
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\d3d12_device_selection_test.exe
