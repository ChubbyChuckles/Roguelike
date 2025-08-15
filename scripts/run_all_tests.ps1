cd build
cmake -G "Visual Studio 17 2022" -A x64 -DROGUE_ENABLE_SDL=ON -DROGUE_SDL2_ROOT="C:/libs/SDL2" ..

# build and run all tests (Release)
cmake --build . --config Release
ctest -C Release --output-on-failure