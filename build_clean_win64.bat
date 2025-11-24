echo off

echo ===START: CLEANING AND SETUP===
if exist %CD%\main.exe (
    del %CD%\main.exe
    echo REMOVED main.exe
)
if not exist %CD%\build\generated\shaders (
    mkdir %CD%\build\generated\shaders
    echo CREATED DIRECTORY build
) else (
    del %CD%\build\generated\shaders\*.h
    echo CLEANED DIRECTORY build/generated/shaders
)
echo ===END: CLEANING AND SETUP===
echo.

echo ===START: CONVERTING SHADERS===
setlocal enabledelayedexpansion
for /R "src" %%F in (*.vert *.frag *.comp) do (
    set "filename=%%~nF"
    set "fileext=%%~xF"
    set "fileext=!fileext:~1!"
    set "outputfilename=!filename!_!fileext!"
    set "compiledfilename=!outputfilename!.spv"
    
    "tools/glslangValidator/glslangValidator.exe" -V --target-env vulkan1.2 %%F -o build/generated/shaders/!compiledfilename! >nul 2>&1
    if not exist %CD%\build\generated\shaders\!compiledfilename! (
        echo SHADER COMPILATION FAILED -- exiting...
        exit
    )

    "tools/xxd/xxd.exe" -i -n !outputfilename! build/generated/shaders/!compiledfilename! > build/generated/shaders/!outputfilename!.h
    if not exist %CD%\build\generated\shaders\!outputfilename!.h (
        echo SHADER TO HEADER CONVERSION FAILED -- exiting...
        exit
    )

    echo COMPILED SHADER AND CONVERTED TO HEADER !filename!.!fileext! ~ !outputfilename!.h
)
echo ===END: CONVERTING SHADERS===
echo.

echo ===START: COMPILING SOURCE CODE===
set "src="
for /R "src" %%F in (*.c) do (
    set "src=!src! %%F"
)
"tools/tcc/tcc.exe"^
 -g^
 -Ibuild/generated/shaders -Ilib/vulkan/include -Ilib/tcc/include -Ilib/tcc/include/winapi^
 -Llib/vulkan/ -lvulkan-1 -luser32 -lGdi32^
 -DWIN -DWIN32 -DWIN64 -DOSBITS=64^
 -DAPP_VERSION=1 -DENGINE_VERSION=1^
 !src! -o main.exe

if exist %CD%\main.exe (
    echo COMPILATION SUCCESSFUL
) else (
    echo COMPILATION FAILED -- exiting...
    exit
)
echo ===END: COMPILING SOURCE CODE===
echo.

echo ===PROGRAM START===
echo on
"main.exe"
@echo off
echo ===PROGRAM ENDED===