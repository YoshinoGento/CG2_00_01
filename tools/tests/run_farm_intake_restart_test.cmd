@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\codex_checks\intake_restart_obj mkdir generated\codex_checks\intake_restart_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /DNDEBUG /DNOMINMAX /W4 /WX /Iproject /Iproject\application /Iproject\engine tools\tests\farm_intake_restart_test.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmDateSystem.cpp project\application\farm\system\FarmDocumentSystem.cpp project\application\farm\system\FarmLayoutSystem.cpp project\application\farm\system\FarmProgressionSystem.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropSelectionSystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp project\engine\io\JsonFile.cpp project\engine\base\Logger.cpp /Fo:generated\codex_checks\intake_restart_obj\ /Fe:generated\codex_checks\farm_intake_restart_test.exe
if errorlevel 1 exit /b %errorlevel%
set RUN_ID=%RANDOM%%RANDOM%%RANDOM%
for %%M in (write read update read-updated) do (
    generated\codex_checks\farm_intake_restart_test.exe %%M %RUN_ID%
    if errorlevel 1 exit /b 1
)
exit /b 0
