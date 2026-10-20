@echo off
setlocal
set "MSBUILD=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
if not exist "%MSBUILD%" (
  echo MSBuild was not found. Install Visual Studio 2019 C++ build tools.
  pause
  exit /b 1
)
"%MSBUILD%" "%~dp0Project10\Project10.vcxproj" /p:Configuration=Release /p:Platform=Win32 /p:SolutionDir="%~dp0.\\" /p:OutDir="%~dp0work\main-build\bin\" /p:IntDir="%~dp0work\main-build\obj\" /m /v:minimal /nologo
if errorlevel 1 (
  pause
  exit /b 1
)
pushd "%~dp0Project10"
"%~dp0work\main-build\bin\Project10.exe" %*
popd
