@echo off
rem ============================================================
rem  build.bat - builds HandBoard
rem  Used by rregullo.bat and by VS Code (Ctrl+Shift+B).
rem ============================================================
setlocal
cd /d "%~dp0"
set "NOPAUSE=%~1"
set "OCV=C:\opencv-4.14.0\opencv\build"

if not exist "%OCV%\OpenCVConfig.cmake" goto noocv
if not exist "models\palm_detection_mediapipe_2023feb.onnx" goto nomodels
if not exist "models\handpose_estimation_mediapipe_2023feb.onnx" goto nomodels

call :findvc
if not defined VCPATH goto novc
call "%VCPATH%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl >nul 2>nul
if errorlevel 1 goto novc

set "CMAKE=%ProgramFiles%\CMake\bin\cmake.exe"
if not exist "%CMAKE%" set "CMAKE=cmake"

rem if build/ was created with a different generator, start over
if not exist "build\CMakeCache.txt" goto configure
findstr /c:"CMAKE_GENERATOR:INTERNAL=NMake Makefiles" "build\CMakeCache.txt" >nul
if errorlevel 1 rmdir /s /q "build"

:configure
"%CMAKE%" -S . -B build -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release "-DOpenCV_DIR=%OCV%"
if errorlevel 1 goto fail
"%CMAKE%" --build build
if errorlevel 1 goto fail
echo.
echo OK: build\Release\HandBoard.exe  (me opencv_world dhe models prane)
if "%NOPAUSE%"=="" pause
exit /b 0

:noocv
echo GABIM: OpenCV nuk u gjet te %OCV%
echo Hape rregullo.bat qe ta instaloje.
if "%NOPAUSE%"=="" pause
exit /b 1

:nomodels
echo GABIM: mungojne modelet ne dosjen "models". Hape rregullo.bat.
if "%NOPAUSE%"=="" pause
exit /b 1

:novc
echo GABIM: kompajleri C++ (Visual Studio Build Tools) nuk u gjet ose eshte i paplote.
echo Hape rregullo.bat qe ta instaloje / perfundoje.
if "%NOPAUSE%"=="" pause
exit /b 1

:fail
echo GABIM gjate ndertimit (shih tekstin me lart).
if "%NOPAUSE%"=="" pause
exit /b 1

:findvc
rem Finds a Visual Studio / Build Tools install that has the compiler and the Windows SDK.
set "VCPATH="
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VSLIST=%TEMP%\handboard_vs.txt"
echo %ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools> "%VSLIST%"
if exist "%VSWHERE%" "%VSWHERE%" -all -prerelease -products * -property installationPath >> "%VSLIST%" 2>nul
for /f "usebackq delims=" %%i in ("%VSLIST%") do if exist "%%i\VC\Auxiliary\Build\vcvars64.bat" set "VCPATH=%%i"
del "%VSLIST%" >nul 2>nul
if not exist "%ProgramFiles(x86)%\Windows Kits\10\Include" set "VCPATH="
exit /b 0
