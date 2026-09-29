@echo off
setlocal
cd /d "%~dp0..\.."
set "SAVED_PATH=%PATH%"
set Path=
set "PATH=%SAVED_PATH%"
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
set "DEP=generated\kurihaku_20260928\known_dependencies"
cl /nologo /std:c++20 /EHsc /MT /O2 /W4 /WX /utf-8 /I%DEP%\assimp-build\include /I%DEP%\assimp-source\include submission\kurihaku_20260928\verify_official_assimp.cpp %DEP%\assimp-build\lib\assimp-vc145-mt.lib %DEP%\assimp-build\contrib\zlib\zlibstatic.lib /Fo:%DEP%\verify_assimp.obj /Fe:%DEP%\verify_assimp.exe
if errorlevel 1 exit /b %errorlevel%
%DEP%\verify_assimp.exe generated\kurihaku_20260928\asset_clean_03\SuidoNogyo\Resources
exit /b %errorlevel%
