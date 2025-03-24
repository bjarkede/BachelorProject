# [Bachelor Project](http://github.com/bjarkede/) - Toy renderer

This is a toy/experimental real-time global illumination renderer written in C++ and using Direct3D 11. It's a hobby project, which means that it sometimes chooses the simplest options, and is perpetually a work in progress. 

It started as part of my [bachelor thesis](https://bjarkede.com/documents/bachelor_thesis_bjarke_damsgaard_eriksen.pdf) where the goal was to create a real-time renderer that supported global illumination.

![](https://bjarkede.com/gi_overview/screenshots/image1_hu88218d72c5defce792037be31f71092d_186361_1600x1600_fit_q50_box.jpg)

## Features

* Deferred rendering
* Dynamic global illumination
    * Voxel cone traced diffuse lighting and specular reflection using anisotropic voxels
    * Compute shader pre-pass for "infinite" bounces
    * Screen-space specular occlusion to mitigate light leaking
    * Hardware and software conservative rasterization modes
    * Gap-filling compute pass to reduce light leaking
* Ambient occlusion
* PCF and PCSS shadows
* Emissive materials
* Normal mapping
* Material editor
* Built-in GPU timings
* Voxel visualizer

## Project Overview

The project currently consists of:

1. A renderer, `Bachelor`, written in C++ using the Win32 API and Direct3D 11. Supported OSes: Windows only 
2. A command-line tool, `MeshBaker`, which can parse .obj files to our custom format, written in C++. Supported OSes: Windows only
3. A command-line tool, `Hemisphere`, that can generate cones to cover the hemisphere, witten in C++. Supported OSes: Windows only

## Installation

No installation is required for any of the binaries.  
The command-line tools have no third-party dependencies.

`bin\bachelor.exe` requires Direct3D 11 and needs to be able to read from the `bachelor\` folder that contains the assets.

## Controls


* WSAD, Space, Ctrl - movement
* Mouse + Right mouse button- rotate the camera
* Middle mouse button - move slower
* F1 - toggle help window

## Technical Notes

A lot of the technical details are covered in my [thesis](https://bjarkede.com/documents/bachelor_thesis_bjarke_damsgaard_eriksen.pdf), see Chapter 4 (Architecture) and Chapter 5 (Design and Implementation). Chapter 7 (Results and Dicussion) analyzes the performance of the different render passes.

In addition, there's a [global illumination overview](https://bjarkede.com/posts/2022/bachelor_thesis_gi_overview/) post on my website.

## Building from Source

### Windows + Visual C++

The steps:

1. You will need **Visual C++ 2013** or later installed.
2. Navigate to `bachelor\misc`.
3. Run `gen.exe` to generate Visual Studio project files for VS2013/2019<sup>[a]</sup>.
4. Navigate to `vs2013` or `vs2019` and  compile the project.

a) Alternatively you can generate your own project files for other Visual Studio versions using `genie.exe`.

## Notable Known Issues

* Bachelor

   * Voxel data structure is a dense grid. Could experiment with other structures for storing voxels, such as sparse voxel octress or cascaded voxel grids.
   * Short/long cone split in the bounce pass.<sup>[McL15]</sup>
   * The bounce pass can be amortized over multiple frames to reduce the per frame computional expense.
   * Could trace cones for the bounce pass at a lower resolution and upscale.
   * The indirect diffuse contributes to a local fragment's specular response. Need to use another data structure to achieve true indirect specular.

   [McL15] As described by James McLaren. ‘The Technology of The Tomorrow Children’. In:
   GDC (April 2015).

* Meshbaker

   * The parser needs more work to make all .obj readable by `bachelor`.

## Contact

Personal website: [https://bjarkede.com](https://bjarkede.com)  
GitHub user: [bjarkede](https://github.com/bjarkede)

Bjarke#3697 @ [Discord](https://discord.me/CPMA)

## Thanks

* [myT](https://github.com/mightycow) ([bitbucket](https://bitbucket.org/CPMADevs/))
* Sab0o

## License

[BSD 3-Clause License](./LICENSE)
