# Quickstart

## macOS

Run these commands from this direcotry:
```

# install OpenCV
brew install opencv

# build project and provide path to OpenCV
cmake -B build -S .
cmake --build build/

# run the binary
build/bottle-recognizer samples/1.mp4
```