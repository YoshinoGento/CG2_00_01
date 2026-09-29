@echo off
pushd "%~dp0.."
if not exist "%~dp0generated\outputs\Release\CG2_00_01.exe" (
  echo Build Source/project/CG2_00_01.sln in Release x64 first.
  popd
  exit /b 1
)
start "" /wait "%~dp0generated\outputs\Release\CG2_00_01.exe"
popd
