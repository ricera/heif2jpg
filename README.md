Build and Install
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

Future
===
- Handle metadata; right now none of it is transferred. Use ExifTool for this.
- Handle more heif formats; e.g. 4:2:2 images or images from my phone
- As part of the above, grab encoding parameters for the jpeg from the metadata
contained in the heif file; these include things like the subsampling level (e.g. 4:2:0), colorspace (e.g. BT 2100) and transfer function (e.g. HLG)
- Perform anamorphic desqueeze