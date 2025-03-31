rm -rf build
mkdir build
cd build

cmake ..
cmake --build . --config Release

@REM .\Release\logger.exe
