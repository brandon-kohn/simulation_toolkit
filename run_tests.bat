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
CALL :lo_case CONFIG
set CONFIG
echo running %CONFIG% tests.
set CMAKE_GENERATOR=%2

REM Build the release
cmake -H. -Bcmake.build.%CONFIG% -DBUILD_TESTS=1 -DCMAKE_BUILD_TYPE=%CONFIG% -DCMAKE_INSTALL_PREFIX="cmake.install" -G%CMAKE_GENERATOR%
if ERRORLEVEL 1 goto error

REM Build it
cmake --build cmake.build.%CONFIG% --config %CONFIG%
if ERRORLEVEL 1 goto error

cd cmake.build.%CONFIG%
ctest -C %CONFIG% %3 %4 %5 %6 %7 %8 %9
if ERRORLEVEL 1 goto error

exit /B 0

:lo_case
:: Subroutine to convert a variable VALUE to all lower case.
:: The argument for this subroutine is the variable NAME.
for %%i IN ("A=a" "B=b" "C=c" "D=d" "E=e" "F=f" "G=g" "H=h" "I=i" "J=j" "K=k" "L=l" "M=m" "N=n" "O=o" "P=p" "Q=q" "R=r" "S=s" "T=t" "U=u" "V=v" "W=w" "X=x" "Y=y" "Z=z") DO CALL SET "%1=%%%1:%%~i%%"
goto:eof

:usage
@echo Usage: run_tests.bat ^<Config^> ^<"CMake Generator"^>
exit /B 1

:error
exit /B 1

REM End