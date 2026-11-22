@echo off
rem Local build. Needs: Visual Studio 2022 (Desktop C++ workload), CMake and Git on PATH.
cmake -B build -G "Visual Studio 17 2022" -A x64 || goto :err
cmake --build build --config Release --target autoladd_VST3 --parallel || goto :err
echo.
echo Built: build\autoladd_artefacts\Release\VST3\autoladd.vst3
echo Copy that folder to "%CommonProgramFiles%\VST3" (or run Scripts\install-windows.bat from a folder containing it).
exit /b 0
:err
echo Build failed.
exit /b 1
