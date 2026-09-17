@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\crop_visual_obj mkdir generated\codex_checks\crop_visual_obj
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /DNOMINMAX /Iproject /Iproject\application /Iproject\engine tools\tests\farm_crop_visual_size_test.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmVisualSystem.cpp project\application\farm\system\FarmIrrigationSystem.cpp /Fo:generated\codex_checks\crop_visual_obj\ /Fe:generated\codex_checks\farm_crop_visual_size_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_crop_visual_size_test.exe
