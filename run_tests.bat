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
set REPOSRC=https://github.com/brandon-kohn/simulation_suite.git

git pull "%REPOSRC%"
if ERRORLEVEL 1 goto error
git submodule update --init --recursive
if ERRORLEVEL 1 goto error

set CONFIG=%1
set CMAKE_GENERATOR=%2

REM Build the release
cmake -H. -Bcmake.build -DBUILD_TESTS=1 -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_INSTALL_PREFIX="cmake.install" -G%CMAKE_GENERATOR%
if ERRORLEVEL 1 goto error

REM Build it
cmake --build "cmake.build" --config %CONFIG%
if ERRORLEVEL 1 goto error

cd cmake.build
ctest -C %CONFIG% %3 %4 %5 %6 %7 %8 %9
if ERRORLEVEL 1 goto error

exit /B 0

:usage
@echo Usage: run_tests.bat ^<Config^> ^<"CMake Generator"^>
exit /B 1

:error
exit /B 1

REM End