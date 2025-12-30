# Quickstart

## macOS

Run these commands from this direcotry:
```
brew install opencv
cmake -B build -S .
cmake --build build/
build/bottle-recognizer samples/1.mp4
ffplay output.mp4
```

## Windows

### Prerequisites
1. Install [CMake](https://cmake.org/download/) (version 3.10+)
2. Install [Visual Studio 2022](https://visualstudio.microsoft.com/) with C++ tools
3. Install [OpenCV](https://github.com/opencv/opencv) to `C:/lib/install/opencv/`

### Build & Run
From the `bottle-recognizer` directory:
```
mkdir build
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
cd Release
export PATH=$PATH:/c/lib/install/opencv/x64/vc17/bin
./bottle-recognizer.exe ../samples/1.mp4
```

The output video will be saved as `output.mp4` in the `Release` directory.