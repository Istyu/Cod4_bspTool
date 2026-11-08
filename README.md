# Cod4 BSP Tool (Enhanced for Call of Duty 4)

A modified and fixed version of the classic Quake/RTCW BSPC tool, specifically tailored and corrected for **Call of Duty 4: Modern Warfare** map and data extraction.

---

## 🛠️ Recent Fixes & Improvements (CoD4 Specific)

Unlike the original BSPC 2.1c (which often generated broken Quake 3 format maps causing assertions or empty brushes in CoD4 Radiant), this enhanced version includes critical fixes:
- **Proper `iwmap 4` Format:** Generates valid CoD4 map headers and 17-field brush-face structures preserving original shader names (e.g., `ch_brick_wall_04`, `clip_player`).
- **Double-Precision Bounds:** Raised coordinate limits beyond the old $\pm8192$ threshold up to $\pm65536$ to prevent the loss or corruption of large structures.
- **Robust Memory Handling:** Clears uninitialized memory blocks to prevent random parser crashes (such as `MAX_PATCH_VERT` or invalid patch sizes).
- **Entity & Model Placeholders:** Automatically generates valid `nodraw_notsolid` placeholder brush boxes for rotating `script_brushmodel` entities lacking collision geometry, avoiding Radiant startup assertions.
- **Clean Output:** Eliminates duplicate worldspawn keys and filters out zero-face / degenerate brushes.

---

## License & Origin

Return to Castle Wolfenstein multiplayer GPL Source Code  
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company. 

This file is part of the Return to Castle Wolfenstein multiplayer GPL Source Code (RTCW MP Source Code).  

*(The software is distributed under the terms of the GNU General Public License v3 or later - see [Releases](http://www.gnu.org/licenses/)).*

This project is part of the [CoD4x Server](https://github.com/callofduty4x/CoD4x_Server) ecosystem.

---

## Compiling on Windows

To compile CoD4x from source you require the following tools:

- [NASM](http://www.nasm.us/pub/nasm/releasebuilds/?C=M;O=D)
- [MinGW](http://www.mingw.org/)

Now compile the tool by running `build_win32_PE.cmd`.

If compilation was successful the binary will be placed in the /bin/ folder. 

---

## How to use BSP tool

Usage:
- `bspc [-<switch> [-<switch> ...]]`
- **Example 1**: `bspc -bsp2aas d:\\quake3\\baseq3\\maps\\mymap?.bsp`  
- **Example 2**: `bspc -bsp2aas d:\\quake3\\baseq3\\pak0.pk3\\maps/q3dm*.bsp`

Switches: 
- `bsp2map            <[pakfilter/]filter.bsp>     `convert BSP or FF to MAP
- `bsp2aas            <[pakfilter/]filter.bsp>     `convert BSP to AAS
- `reach              <filter.bsp>                 `compute reachability & clusters
- `cluster            <filter.aas>                 `compute clusters
- `aasopt             <filter.aas>                 `optimize aas file
- `output             <output path>                `set output path
- `threads            <X>                          `set number of threads to X
- `cfg                <filename>                   `use this cfg file
- `optimize                                        `enable optimization
- `noverbose                                       `disable verbose output
- `breadthfirst                                    `breadth first bsp building
- `nobrushmerge                                    `don't merge brushes
- `noliquids                                       `don't write liquids to map
- `freetree                                        `free the bsp tree
- `nocsg                                           `disables brush chopping
- `forcesidesvisible                               `force all sides to be visible
- `grapplereach                                    `calculate grapple reachabilities

---

### Tool Metadata 

- Name:&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;BSP tool
- Programmer:&emsp;&emsp;&emsp;&emsp;id Software & Mr Elusive (MrElusive@demigod.demon.nl)
- Last update:&emsp;&emsp;&emsp;&emsp;1997-12-04
- Tab Size:&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;3
- Notes:&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;&emsp;Microsoft Visual C++ optimizations: "global optimization" or "full optimization" results in micro brushes??
