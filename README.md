# myEQ

![Platforms](https://img.shields.io/badge/platform-macOS%20%7C%20Windows%20%7C%20Linux-brightgreen.svg)
![JUCE](https://img.shields.io/badge/made%20with-JUCE-brightgreen.svg)
![CMake](https://img.shields.io/badge/build%20system-CMake-blue.svg)
[![Python Version](https://img.shields.io/badge/python-3.6%2B-blue.svg)](https://www.python.org/)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

A custom audio EQ plugin built with [JUCE](https://juce.com) and CMake.  
Supports building as VST3, AU, and standalone (depending on your platform).

<p align="center">
  <img src="https://github-production-user-asset-6210df.s3.amazonaws.com/93061186/454814960-0066d071-6ecf-4eea-a9af-21fa915d239f.png?X-Amz-Algorithm=AWS4-HMAC-SHA256&X-Amz-Credential=AKIAVCODYLSA53PQK4ZA%2F20250613%2Fus-east-1%2Fs3%2Faws4_request&X-Amz-Date=20250613T101157Z&X-Amz-Expires=300&X-Amz-Signature=2df2b7612a241e6a84e1a8d583538349354ef9bc55f0ae9a25da29ff19a8d778&X-Amz-SignedHeaders=host" alt="myEQ UI Screenshot" width="600" />
</p>

## 🚀 Features

- Parametric EQ with real-time visualization
- Built using JUCE and modern CMake
- Cross-platform (macOS, Windows, Linux)

## 🛠️ Requirements

### System

| Tool       | Minimum Version | Install Command (macOS/Linux)             |
|------------|------------------|-------------------------------------------|
| CMake      | 3.15+             | `brew install cmake` / `sudo apt install cmake` |
| C++ Compiler | C++17 compatible | See below                                 |

### Platform-Specific

- **macOS**: Xcode + Command Line Tools  
  Install via: `xcode-select --install`
  
- **Windows**: Visual Studio 2019 or newer (with C++ workload)
  
- **Linux**: `g++` or `clang`, ALSA or JACK development headers may be required

## 🧪 Building the Plugin

This project uses a Python helper script to configure and build the plugin using CMake.

### Quick Start

```bash
# Clone the repository
git clone --recurse-submodules https://github.com/nablum/myEQ.git
cd myEQ

# Run the build script (Release build by default)
python3 build.py
```

### Optional arguments
```bash
python3 build.py \
    --config Debug \
    --generator Ninja \
    --parallel 4
```

| Option        | Description                                                |
| ------------- | ---------------------------------------------------------- |
| `--config`    | Build type: `Release` (default), `Debug`, `RelWithDebInfo` |
| `--generator` | Use a specific CMake generator (e.g., `Ninja`, `Xcode`)    |
| `--parallel`  | Number of parallel build jobs                              |

## 📦 Output

After building, the plugin is automatically copied to your system's plugin folder.

This is handled by the JUCE CMake option:

```cmake
COPY_PLUGIN_AFTER_BUILD TRUE
```
| Format | macOS                                  | Windows                      | Linux                          |
| ------ | -------------------------------------- | ---------------------------- | ------------------------------ |
| VST3   | `~/Library/Audio/Plug-Ins/VST3/`       | `%COMMONPROGRAMFILES%\VST3\` | `~/.vst3/` or `/usr/lib/vst3/` |
| AU     | `~/Library/Audio/Plug-Ins/Components/` | —                            | —                              |

## ⚠️ Plugin Format
By default, the plugin is built only in VST3 format, as specified in the `CMakeLists.txt` file:
```bash
juce_add_plugin(myEQ
    ...
    FORMATS VST3
)
```

If you want to build other formats (e.g. AU, AAX, Standalone), update the `FORMATS` list:

```bash
FORMATS VST3 AU Standalone
```
Make sure your system supports the additional formats and JUCE is properly configured (e.g., Xcode for AU, AAX SDK for AAX).

## 🧼 Clean Build
Remove previous build files and build fresh (useful if build errors occur):
```bash
rm -rf build
python3 build.py
```

## 📚 Resources

- [JUCE Documentation](https://docs.juce.com/)
- [CMake Tutorial](https://cmake.org/cmake/help/latest/guide/tutorial/index.html)

## 📄 License

This project is released under the [MIT License](LICENSE).

JUCE is © [ROLI Ltd. / SoundStax](https://juce.com) and distributed under their terms.  
Make sure to comply with JUCE's licensing model if you distribute this plugin.

## 🙋‍♂️ Contact

Created by [@nablum](https://github.com/nablum)

Feel free to open [issues](https://github.com/nablum/myEQ/issues) or submit pull requests if you find bugs or have feature suggestions!
