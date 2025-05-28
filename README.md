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

These will create a `build` directory in the repository root, populate it
with CMake configuration, pull in third-party dependencies into folders inside
`third_party`, build everything, and install heif2jpeg along with any runtime
dependencies. Currently, the FreeBSD version doesn't need any, but the Windows
build adds a couple .dlls.

Installs to `<repo-dir>/install/bin`, but you can overwrite
`CMAKE_INSTALL_PREFIX` in the top-level CMakeLists.txt to change this.