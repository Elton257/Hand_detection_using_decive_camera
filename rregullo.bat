@echo off
setlocal
cd /d "%~dp0"
title HandBoard - rregullimi dhe ndertimi
echo ============================================
echo   HandBoard - rregullimi dhe ndertimi
echo ============================================
echo.

rem ---- 1. VS Code configuration ----
echo [1/6] Konfigurimi i VS Code...
if not exist "vscode-config\tasks.json" goto vscode_ok
if not exist ".vscode" mkdir ".vscode"
copy /y "vscode-config\*" ".vscode\" >nul
rmdir /s /q "vscode-config"
echo       u kopjua te .vscode
:vscode_ok
echo       ne rregull

rem ---- 2. CMake ----
echo [2/6] CMake...
set "CMAKE=%ProgramFiles%\CMake\bin\cmake.exe"
if not exist "%CMAKE%" winget install -e --id Kitware.CMake --accept-package-agreements --accept-source-agreements
if not exist "%CMAKE%" goto nocmake
echo       ne rregull

rem ---- 3. C++ compiler + Windows SDK ----
echo [3/6] Kompajleri C++ dhe Windows SDK...
call :findvc
if defined VCPATH goto vcok
call :repairvc
call :findvc
if defined VCPATH goto vcok
echo.
echo GABIM: kompajleri C++ ende mungon.
echo Rinise kompjuterin dhe hape prape rregullo.bat.
goto fail
:vcok
echo       ne rregull: "%VCPATH%"

rem ---- 4. OpenCV 4.14.0 ----
echo [4/6] OpenCV 4.14.0...
set "OCVROOT=C:\opencv-4.14.0"
set "OCVEXE=%TEMP%\opencv-4.14.0-windows.exe"
if exist "%OCVROOT%\opencv\build\OpenCVConfig.cmake" goto ocvok
echo       po shkarkoj (~200 MB) nga github.com/opencv/opencv ...
curl.exe -L --fail -o "%OCVEXE%" "https://github.com/opencv/opencv/releases/download/4.14.0/opencv-4.14.0-windows.exe"
if errorlevel 1 goto noocv
echo       po e shpaketoj te %OCVROOT% (~1 GB, 1-3 minuta) ...
start "" /wait "%OCVEXE%" -o"%OCVROOT%" -y
if not exist "%OCVROOT%\opencv\build\OpenCVConfig.cmake" goto noocv
del "%OCVEXE%" >nul 2>nul
:ocvok
echo       ne rregull: %OCVROOT%

rem ---- 5. Hand models (OpenCV Zoo, Apache 2.0) ----
echo [5/6] Modelet e duarve...
if not exist "models" mkdir "models"
set "ZOO=https://media.githubusercontent.com/media/opencv/opencv_zoo/main/models"
if not exist "models\palm_detection_mediapipe_2023feb.onnx" curl.exe -L --fail -o "models\palm_detection_mediapipe_2023feb.onnx" "%ZOO%/palm_detection_mediapipe/palm_detection_mediapipe_2023feb.onnx"
if not exist "models\handpose_estimation_mediapipe_2023feb.onnx" curl.exe -L --fail -o "models\handpose_estimation_mediapipe_2023feb.onnx" "%ZOO%/handpose_estimation_mediapipe/handpose_estimation_mediapipe_2023feb.onnx"
if not exist "models\palm_detection_mediapipe_2023feb.onnx" goto nomodels
if not exist "models\handpose_estimation_mediapipe_2023feb.onnx" goto nomodels
echo       ne rregull

rem ---- 6. Build ----
echo [6/6] Ndertimi...
call build.bat nopause
if errorlevel 1 goto fail
echo.
echo ============================================
echo   GATI. Hape: build\Release\HandBoard.exe
echo   Ne VS Code: Ctrl+Shift+B ndertim, F5 nis.
echo ============================================
pause
exit /b 0

:nocmake
echo GABIM: CMake nuk u instalua.
goto fail

:noocv
echo GABIM: OpenCV nuk u shkarkua / shpaketua dot.
echo Kontrollo internetin dhe provo prape.
goto fail

:nomodels
echo GABIM: modelet nuk u shkarkuan dot.
goto fail

:repairvc
set "VSSETUP=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\setup.exe"
set "VSBT=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\BuildTools"
echo       Instalimi i Build Tools eshte i paplote. Po e perfundoj (5-15 min).
echo       Kliko PO kur Windows-i te pyet per leje.
if not exist "%VSSETUP%" goto repair_winget
if not exist "%VSBT%" goto repair_winget
start "" /wait "%VSSETUP%" resume --installPath "%VSBT%" --passive --norestart
start "" /wait "%VSSETUP%" modify --installPath "%VSBT%" --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --norestart
call :findvc
if defined VCPATH exit /b 0
:repair_winget
winget install -e --force --id Microsoft.VisualStudio.2022.BuildTools --accept-package-agreements --accept-source-agreements --override "--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive --wait --norestart"
exit /b 0

:fail
echo.
echo ============================================
echo   Dicka deshtoi. Kopjo tekstin me lart
echo   dhe dergoja Claude-it.
echo ============================================
pause
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
