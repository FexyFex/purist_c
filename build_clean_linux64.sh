exit

echo ===END: CLEANING AND SETUP===
if [ -f ./main.exe]; then
    rm ./main.exe
    echo REMOVED main.exe
fi
if [ ! -d ./build/generated/shaders]; then
    mkdir ./build/generated/shaders
    echo CREATED DIRECTORY build/generated/shaders
else
    rm ./build/generated/shaders/*.h
    echo CLEANED DIRECTORY build/generated/shaders
fi
echo ===END: CLEANING AND SETUP===
echo -

echo ===START: CONVERTING SHADERS===
find ./src -type f | while read file; do
    echo "$file"
    filename=$(basename "$file")
    fileext="${filename##*.}"
    ./tools/xxd/xxd -i -n ${filename}_${fileext} $file > build/generated/${filename}_${fileext}.h
    echo CONVERTED TO HEADER ${filename}.${fileext}
done
echo ===END: CONVERTING SHADERS===
echo -

echo ===START: COMPILING SOURCE CODE===
./compile_unix.sh "UNIX" "UNIX64" "OSBITS=64"
echo BUILD SUCCESSFUL
echo ===END: COMPILING SOURCE CODE===
echo -

echo ===PROGRAM START===
./main

read