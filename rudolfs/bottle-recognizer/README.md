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