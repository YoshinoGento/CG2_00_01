@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\layout_obj mkdir generated\codex_checks\layout_obj
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /D NOMINMAX /Iproject /Iproject\application tools\tests\farm_layout_test.cpp project\application\farm\system\FarmLayoutSystem.cpp project\application\farm\core\FarmGrid.cpp /Fo:generated\codex_checks\layout_obj\ /Fe:generated\codex_checks\farm_layout_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_layout_test.exe
