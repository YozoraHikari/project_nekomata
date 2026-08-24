# Project Nekomata

Just a plain 3D game engine using modern Vulkan.

I don't expect it to be anywhere near ready to make actual stuff with; I mostly intend it to be a learning project.
Well, maybe in the future..

## Current Features

- Multithreading (Main and render thread split)
- ECS-based world
- Basic input handling
- Custom standard library containers and algorithms replacement to bring Rust-style ergonomics
- Code built fully with `-fno-exceptions`
- Mesh rendering
- Mesh LODs
- Async insertion of mesh LODs
- Textures
- Async loading of KTX2 textures
- Bindless textures and samplers
- Dynamic deferred lighting with PBR IBL
- Bitmap font rendering with on-the-fly font atlas generation
- Basic script system
- `VK_KHR_pipeline_binary`-based pipeline caching
- SMAA antialiasing

## Planned Features

- Shadow mapping
- A more full UI solution
- MSDF font rendering
- Model loading (most likely glTF)
- Physics
- `VK_EXT_descriptor_heap`-based texture and sampler tables
- Ray tracing
- Render graph
- Post-processing effects (bloom, etc.)

## Building

Note that the version numbers provided are what the project works with. Earlier or later versions of a given dependency might or might not build/work.

To build the project, you need the following dependencies:
- Base development packages (e.g. `base-devel` on Arch Linux).
- CMake 4.3.4. Not lower, not higher.
- Clang 22 or later (GCC may work but not tested and is not really supported in the long run)
- Vulkan SDK (the project typically requires the very latest version of the SDK but a couple versions back might work fine)
- Slang compiler v2026.11 or later. Older versions might cause issues on AMD graphics cards due to a bug in the Slang compiler with NURI.
- SDL3 3.4.8 or later
- FreeType2 2.14.3 or later
- utfcpp 4.0.9 or later
- tcmalloc 4.6.5 or later (part of gperftools)

On CachyOS, you can install all of the dependencies as follows:
```shell
paru -S base-devel cmake clang sdl3 freetype2 gperftools utf8cpp vulkan-headers shader-slang 
```

Download the project's source code (e.g. using `git clone`). At that point you can either build and run it in your
favourite IDE (e.g. using the CMake integration in CLion) or run the following commands (assuming you're in the project's
root directory):
```shell
mkdir build
cd build
cmake ..
cmake --build .
./projnekomata_main
```

If you generate makefiles instead of a build.ninja file, add `-j(number of cores + 1)` to the `cmake --build .` command to parallelize the build.

## License

Copyright (c) 2026 YozoraHikari

This project is licensed under the Apache 2.0 license. See the LICENSE file for more information.

This project makes use of several third-party works, which are licensed under their respective licenses. See files
[Assets/LICENSE-Inconsolata.md](Assets/LICENSE-Inconsolata.md), [Assets/LICENSE-IosevkaTerm-Light.md](Assets/LICENSE-IosevkaTerm-Light.md),
[Assets/deccer-cubes-main/LICENSE](Assets/deccer-cubes-main/LICENSE) for more information.