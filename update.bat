@echo off
setlocal ENABLEEXTENSIONS

REM `simulation_suite` update and build script.
REM This script does a pull on simulation_suite at the URL below.
REM It will then invoke cmake to update and build the suite installing into the specified path.
REM Requires git and cmake on the path.
REM Note: Also requires the environment is configured for the specified CMAKE generator.

if "%~1"=="" goto usage
if "%~2"=="" goto usage

REM Update the repo
set REPOSRC=https://github.com/brandon-kohn/simulation_toolkit.git

git pull "%REPOSRC%"
if ERRORLEVEL 1 goto error
git submodule update --init --recursive
if ERRORLEVEL 1 goto error

set INSTALL_PATH=%1
set CMAKE_GENERATOR=%2
set ARCHITECTURE=""
if "%~3" neq "" (set ARCHITECTURE=-A"%~3")

REM Build the release
cmake -H. -Bcmake.build.release -DCMAKE_BUILD_TYPE=release -DCMAKE_INSTALL_PREFIX="%INSTALL_PATH%" -G%CMAKE_GENERATOR% %ARCHITECTURE%
if ERRORLEVEL 1 goto error

REM Build it
cmake --build "cmake.build.release" --config release --target install
if ERRORLEVEL 1 goto error

REM Build the debug
cmake -H. -Bcmake.build.debug -DCMAKE_BUILD_TYPE=debug -DCMAKE_INSTALL_PREFIX="%INSTALL_PATH%" -G%CMAKE_GENERATOR% %ARCHITECTURE%
if ERRORLEVEL 1 goto error

REM Build it
cmake --build "cmake.build.debug" --config debug --target install
if ERRORLEVEL 1 goto error

exit /B 0

:usage
@echo Usage: update.bat ^<Install Path Prefix^> ^<"CMake Generator"^>
exit /B 1

:error
exit /B 1

REM End
