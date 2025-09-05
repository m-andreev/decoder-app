#!/bin/bash

echo "Building ISO TP Parser..."

cd cpp_implementation

# Compile the C++ program
g++ -std=c++11 -Wall -Wextra -O2 iso_tp_parser.cpp -o iso_tp_parser

if [ $? -ne 0 ]; then
    echo "Build failed!"
    exit 1
fi

echo "Build successful!"
echo "Running ISO TP Parser..."

# Run the program
./iso_tp_parser

if [ $? -ne 0 ]; then
    echo "Program execution failed!"
    exit 1
fi

echo ""
echo "Program completed successfully!"
echo "Check output.txt for results."

cd ..
