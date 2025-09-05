@echo off
echo Building ISO TP Parser...

cd cpp_implementation

:: Compile the C++ program
g++ -std=c++11 -Wall -Wextra -O2 iso_tp_parser.cpp -o iso_tp_parser.exe

if %ERRORLEVEL% neq 0 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build successful!
echo Running ISO TP Parser...

:: Run the program
iso_tp_parser.exe

if %ERRORLEVEL% neq 0 (
    echo Program execution failed!
    pause
    exit /b 1
)

echo.
echo Program completed successfully!
echo Check output.txt for results.

cd ..
pause
