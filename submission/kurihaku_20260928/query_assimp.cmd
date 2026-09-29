@echo off
setlocal
cd /d "%~dp0..\.."
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++20 /EHsc /MT /Iproject\externals\assimp\include submission\kurihaku_20260928\query_assimp.cpp project\externals\assimp\lib\Release\assimp-vc143-mt.lib /Fo:generated\codex_checks\query_assimp.obj /Fe:generated\codex_checks\query_assimp.exe
if errorlevel 1 exit /b %errorlevel%
generated\codex_checks\query_assimp.exe
