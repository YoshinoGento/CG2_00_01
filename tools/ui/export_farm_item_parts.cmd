@echo off
setlocal
set "ITEM_BUILD_PATH=%PATH%"
set Path=
set "PATH=%ITEM_BUILD_PATH%"
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
if not exist generated\farm_item_icons mkdir generated\farm_item_icons
cl /nologo /std:c++20 /utf-8 /EHsc /W4 /WX /D NOMINMAX /Iproject /Iproject\application /Iproject\engine tools\ui\export_farm_item_parts.cpp /Fo:generated\farm_item_icons\export.obj /Fe:generated\farm_item_icons\export.exe
if errorlevel 1 exit /b %errorlevel%
generated\farm_item_icons\export.exe > generated\farm_item_icons\parts.json
exit /b %errorlevel%
