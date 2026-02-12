@echo off
REM Build script for Windows

setlocal

set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release

set BUILD_DIR=build

echo Building Light3D for Windows...
echo Build type: %BUILD_TYPE%

REM Create build directory
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

REM Configure with CMake using Ninja generator
cmake -G Ninja ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DLIGHT3D_BUILD_EXAMPLES=ON ^
    -DLIGHT3D_ENABLE_OPENGL=ON ^
    ..

if %ERRORLEVEL% NEQ 0 (
    echo CMake configuration failed!
    exit /b %ERRORLEVEL%
)

REM Build
ninja

if %ERRORLEVEL% NEQ 0 (
    echo Build failed!
    exit /b %ERRORLEVEL%
)

echo.
echo Build complete!
echo Binary location: %BUILD_DIR%\examples\simple_example.exe

endlocal
