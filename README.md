Build
===

- Uses cmake
- Optionally install nasm for faster jpeg encoding

Windows:
```
cmake -G "Visual Studio 17 2022" -S ./ -B build
cmake --build build --config Release
cmake --install build
```

FreeBSD (I've tested this with Ninja):
```
cmake -G "Ninja" -S ./ -B build
cmake --build build --config Release
cmake --install build
```
