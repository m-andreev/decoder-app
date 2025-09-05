@echo off
setlocal EnableExtensions

set "SRC=parser.cpp"
set "EXE=parser.exe"

if not exist "%SRC%" goto NOSRC

where cl.exe >nul 2>&1
if errorlevel 1 goto TRY_GPP

echo Building with MSVC cl
cl /nologo /EHsc /O2 "%SRC%" /Fe:%EXE%" >nul
if errorlevel 1 goto BUILD_FAIL
goto RUN

:TRY_GPP
where g++.exe >nul 2>&1
if errorlevel 1 goto NOCOMP

echo Building with g++
g++ -std=gnu++17 -O2 -o "%EXE%" "%SRC%"
if errorlevel 1 goto BUILD_FAIL
goto RUN

:RUN
if not exist "transcript.txt" goto NOINPUT
echo Running
"%EXE%" "transcript.txt" > "output.txt"
if errorlevel 1 goto RUN_FAIL
echo Done. Results saved to output.txt
goto END

:NOSRC
echo Source file parser.cpp not found.
exit /b 1

:NOCOMP
echo No C++ compiler found. Install MSVC Build Tools or MinGW-w64.
exit /b 1

:NOINPUT
echo Input file transcript.txt not found next to the script.
exit /b 1

:BUILD_FAIL
echo Build failed.
exit /b 1

:RUN_FAIL
echo Program reported an error.
exit /b 1

:END
exit /b 0
