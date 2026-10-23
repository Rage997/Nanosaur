# How to build Nanosaur

## The easy way: build.py (automated build script)

`build.py` can produce a game executable from a fresh clone of the repo in a single command. It will work on macOS, Windows and Linux, provided that your system has Python 3, CMake, and an adequate C++ compiler.

```
git clone --branch main --recurse-submodules https://github.com/Rage997/Nanosaur
cd Nanosaur
python3 build.py
```

If you want to build the game **manually** instead, the rest of this document describes how to do just that on each of the big 3 desktop operating systems.

### Pomme dependency

Pomme remains pinned to the upstream Git submodule. `python3 build.py` prepares it
automatically during its dependencies step. For manual native or web builds, run
`python3 prepare_dependencies.py` before configuring CMake.

This explicit setup initializes the submodule and applies
[`patches/pomme-reuse-sdl-target.patch`](patches/pomme-reuse-sdl-target.patch), allowing
Pomme to reuse the parent project's `SDL3::SDL3` target. 

## How to build the game manually on macOS

1. Install the prerequisites:
    - Xcode (preferably the latest version)
    - [CMake](https://formulae.brew.sh/formula/cmake) 3.21+ (installing via Homebrew is recommended)
1. Clone the repo **recursively**:
    ```
    git clone --branch main --recurse-submodules https://github.com/Rage997/Nanosaur
    cd Nanosaur
    python3 prepare_dependencies.py
    ```
1. Download [SDL3-3.2.8.dmg](https://libsdl.org/release/SDL3-3.2.8.dmg), open it, then browse to SDL3.xcframework/macos-arm64_x86_64. In that folder, copy **SDL3.framework** to the game's **extern** folder.
1. Prep the Xcode project:
    ```
    cmake -G Xcode -S . -B build
    ```
1. Now you can open `build/Nanosaur.xcodeproj` in Xcode, or you can just go ahead and build the game:
    ```
    cmake --build build --config RelWithDebInfo
    ```
1. The game gets built in `build/RelWithDebInfo/Nanosaur.app`. Enjoy!

## How to build the game manually on Windows

1. Install the prerequisites:
    - Visual Studio 2022 with the C++ toolchain
    - [CMake](https://cmake.org/download/) 3.21+
1. Clone the repo **recursively**:
    ```
    git clone --branch main --recurse-submodules https://github.com/Rage997/Nanosaur
    cd Nanosaur
    python3 prepare_dependencies.py
    ```
1. Download [SDL3-devel-3.2.8-VC.zip](https://libsdl.org/release/SDL3-devel-3.2.8-VC.zip), extract it, and copy **SDL3-3.2.8** to the **extern** folder. Rename **SDL3-3.2.8** to just **SDL3**.
1. Prep the Visual Studio solution:
    ```
    cmake -G "Visual Studio 17 2022" -A x64 -S . -B build
    ```
1. Now you can open `build/Nanosaur.sln` in Visual Studio, or you can just go ahead and build the game:
    ```
    cmake --build build --config Release
    ```
1. The game gets built in `build/Release/Nanosaur.exe`. Enjoy!

## How to build the game manually on Linux et al.

1. Install the prerequisites from your package manager:
    - Any C++20 compiler
    - CMake 3.21+
    - SDL3 development library (e.g. "libsdl3-dev" on Ubuntu, "sdl3" on Arch, "SDL3-devel" on Fedora)
    - OpenGL development libraries (e.g. "libgl1-mesa-dev" on Ubuntu)
1. Clone the repo **recursively**:
    ```
    git clone --branch main --recurse-submodules https://github.com/Rage997/Nanosaur
    cd Nanosaur
    python3 prepare_dependencies.py
    ```
1. Build the game:
    ```
    cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
    cmake --build build
    ```
    If you'd like to enable runtime sanitizers, append `-DSANITIZE=1` to the **first** `cmake` call above.
1. The game gets built in `build/Nanosaur`. Enjoy!

## WebAssembly / WebGL2

From the repository root, with [Emscripten](https://emscripten.org/docs/getting_started/downloads.html)
installed and activated, CMake 3.21+, and Python 3:

```sh
python3 prepare_dependencies.py
emcmake cmake -S . -B build-web -DCMAKE_BUILD_TYPE=Release
cmake --build build-web -j
python3 -m http.server 8000 --directory build-web
```

Open **http://localhost:8000/Nanosaur.html**. CMake fetches and builds pinned SDL 3.2.28;
no desktop SDL installation is needed. To reuse a local SDL source checkout, add
`-DFETCHCONTENT_SOURCE_DIR_SDL3=/path/to/SDL` to the configure command.

### Hosting

Deploy these build outputs together on a static HTTPS server:

- `Nanosaur.html`, `Nanosaur.js`, `Nanosaur.wasm`, `Nanosaur.data`, `Nanosaur.png`
- The complete `web-assets/` directory

Serve `.wasm` as `application/wasm`. No application server or cross-origin isolation
headers are required.

