@echo off
setlocal
set "SUBMISSION_PATH=%PATH%"
set Path=
set "PATH=%SUBMISSION_PATH%"
set "MSBUILD=C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
  echo Visual Studio 2026 Community with v145 is required.
  exit /b 1
)
"%MSBUILD%" "%~dp0project\CG2_00_01.sln" /t:Build /p:Configuration=Release /p:Platform=x64 /p:CL_MPCount=2 /m:2 /nr:false /v:normal /nologo
exit /b %errorlevel%
