# i3-autotiling-c

Written in C, uses practically zero ram. Works with i3. More tiling behaviours will be implemented in the future.
Inspired by [nwg-piotr](https://github.com/nwg-piotr/autotiling) and [suyjuris](https://github.com/suyjuris/i3ipc-simple).

| ![Demo](./assets/Demo.gif) | ![Demo without](./assets/Demo-without.gif) |
| :---: | :---: |
| With Autotiling | Without Autotiling |

*This is made for optimization enthusiasts but for normal users too. An autotiler should not consume more ram than needed since it 
runs in the background.*


**Important Note**: Special thanks to both nwg-piotr and suyjuris for their work, for giving me the inspiration but for also having
good documentation making this project possible.

---
## Contents:
1. [Installation](#Installation)
    - [Build from source](#Build-from-source)
    - [How To Uninstall](#How-To-Uninstall)
2. [Usage](#Usage)
3. [Practically Zero Ram Implementation](#Practically-Zero-Ram-Implementation)

---
## Installation
(*If you are not on an x86_64 machine then see [Build from source](#Build-from-source).*)

Run these commands:
```bash
# 1. Download the binary
wget https://raw.githubusercontent.com/stolen-lighter/i3-autotiling-c/main/bin/i3-autotiling-c

# 2. Make it executable
chmod +x i3-autotiling-c

# 3. Move it to your PATH
mkdir -p ~/.local/bin
mv i3-autotiling-c ~/.local/bin/
```

---
## Build-from-source
### Prerequisites
The makefile autodetects The architecture and has 2 basic categories:
1. x86_64-linux-musl  (for x86_64)
2. aarch64-linux-musl (for arm64, aarch64)

*If none of these is the target architecture we don't pass this flag and let the compiler handle it itself*
Most probably will work on other architectures too, but i don't have the hardware to test it.

**Important !!**
To compile this project as intended, you need the **Zig** installed on your system.
(The makefile has a warning)
#### Build commands
```bash
# Clone the repo
git clone git@github.com:stolen-lighter/i3-autotiling-c.git

# Enter project directory
cd i3-autotiling-c

# Compile and install
make install
```

---
## How To Uninstall
### Uninstall commands
```bash
# Enter project directory
cd i3-autotiling-c

# Run the uninstall command
make uninstall
```

## Usage
To start the script just type in your terminal:
```bash
$ i3-autotiling-c
```
*(If this does not work, check that the folder ~/.local/bin is in your PATH)*

If you want it to start with i3 add it to your config file *(most likely located here: `~/.config/i3/`)*,
like this:
```
# < The rest of the config file >
# .....

exec_always --no-startup-id i3-autotiling-c
# .....
```

## Practically Zero Ram Implementation

The Practically Zero Ram Implementation claim is made due to the fact that this is a standalone static binary file.
With Zero extra memory allocations and heavily optimized functions while the program runs it holds up `32Kb` of RAM.
This means only 8 memory pages given by the kernel to the program.
