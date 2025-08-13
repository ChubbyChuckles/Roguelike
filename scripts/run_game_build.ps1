#!/usr/bin/env pwsh

Set-Location build
cmake --build . --config Release --target rogue_core
cmake --build . --config Release --target roguelike
# ./Release/roguelike.exe
