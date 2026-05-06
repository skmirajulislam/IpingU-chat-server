#!/bin/bash

# Check if build directory exists, create if not
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir -p build
fi

# Navigate to build directory
cd build || exit 1

# Run cmake
echo "Running cmake..."
cmake ..

# Check if cmake was successful
if [ $? -ne 0 ]; then
    echo "CMake failed!"
    exit 1
fi

# Run make
echo "Running make..."
make

# Check if make was successful
if [ $? -ne 0 ]; then
    echo "Make failed!"
    exit 1
fi

# Navigate back to root directory
cd ..

# Run the server
echo "Running server..."
./server
