@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\water_guidance_obj mkdir generated\codex_checks\water_guidance_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /DNDEBUG /W4 /WX /Iproject\application /Iproject\engine tools\tests\farm_water_guidance_test.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmGrowthSystem.cpp /Fo:generated\codex_checks\water_guidance_obj\ /Fe:generated\codex_checks\farm_water_guidance_test.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\farm_water_guidance_test.exe
