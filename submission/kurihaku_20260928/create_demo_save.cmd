@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\kurihaku_20260928\demo_obj mkdir generated\kurihaku_20260928\demo_obj
cl /nologo /std:c++20 /utf-8 /EHsc /O2 /W4 /WX /Iproject /Iproject\application /Iproject\engine submission\kurihaku_20260928\create_demo_save.cpp project\application\farm\core\FarmGrid.cpp project\application\farm\system\FarmDateSystem.cpp project\application\farm\system\FarmDocumentSystem.cpp project\application\farm\system\FarmProgressionSystem.cpp project\application\farm\system\FarmEconomySystem.cpp project\application\farm\system\FarmCropSelectionSystem.cpp project\application\farm\system\FarmCropQualitySystem.cpp project\engine\io\JsonFile.cpp project\engine\base\Logger.cpp /Fo:generated\kurihaku_20260928\demo_obj\ /Fe:generated\kurihaku_20260928\create_demo_save.exe
if errorlevel 1 exit /b %errorlevel%
if "%~1"=="" (
  generated\kurihaku_20260928\create_demo_save.exe generated\kurihaku_20260928\demo_preset\farm
) else (
  generated\kurihaku_20260928\create_demo_save.exe "%~1" %2
)
