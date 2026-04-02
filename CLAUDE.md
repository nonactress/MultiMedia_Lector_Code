# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Visual Studio C++ project for multimedia programming, specifically focused on image processing with OpenCV. The main focus is on the **Prokudin-Gorsky Color Restoration** assignment (Assignment 2), which reconstructs color images from three vertically stacked grayscale channel images using SSD (Sum of Squared Differences) alignment.

### Key Technologies
- **Language**: C++ (C++11/14, Visual Studio 2022 compatible)
- **Build System**: Visual Studio (MSVC v143) with MSBuild
- **Main Dependency**: OpenCV 2.3.0 (legacy IplImage API, not the modern Mat API)
- **Platforms**: Win32 (x86) and x64
- **Build Configurations**: Debug and Release

## File Organization

All source files are located in the `multimedia/` directory:

### Active Project
- **`howtofindSSD.cpp`** — **Main file** (currently active build target)
  - Implements SSD-based channel alignment for Prokudin-Gorsky color restoration
  - Brute-force search for optimal (dx, dy) offsets

### Prokudin-Gorsky Color Restoration Variants
Multiple implementations exploring different approaches (see `Assignment2_Summary.md` for detailed documentation):
- **`asdf.cpp`** — Baseline working version with val[0] direct comparison
- **`assignment2_hint.cpp`** — Variance-based boundary detection + SSD alignment
- **`DP_Assignment2_Variance.cpp`** — Variance-focused boundary detection
- **`DP_Assignment2_FindVelley.cpp`** — Valley-finding approach for boundaries
- **`DP_Assignment2_Clean.cpp`** — Cleaned-up variant
- **`DP_Assignment2_v2.cpp`** — Alternative implementation

### Other Files
- **Assignment Files**: `DP_Assignment1.cpp` — other formal assignment
- **Weekly Exercises**: `week2-1.cpp`, `week3-1.cpp`, `week3-2.cpp`, `week4-1.cpp`, `week4-2.cpp`, `week5-1.cpp`, `week5-2.cpp`
- **Utilities**: `FileName.cpp`, `adjBriAndContrast.cpp` — helper utilities
- **Documentation**: `Assignment2_Summary.md` — detailed project analysis and approach documentation

Currently, `howtofindSSD.cpp` is the active build target. All other `.cpp` files are excluded from the Debug|Win32 build configuration.

## Building and Running

### Build in Visual Studio
1. Open `multimedia.sln` in Visual Studio 2022 or later
2. Select the desired `.cpp` file as the active target by toggling its `ExcludedFromBuild` property in the `.vcxproj` file
3. Build the solution (Ctrl+Shift+B) or run (F5)

### Build via Command Line (MSBuild)
```bash
# Debug x64 build
msbuild multimedia.sln /p:Configuration=Debug /p:Platform=x64

# Release x64 build
msbuild multimedia.sln /p:Configuration=Release /p:Platform=x64
```

### Running Executables
The compiled executables are generated in the `Debug/` directory:
```bash
./Debug/multimedia.exe
```

## OpenCV Setup

The project depends on OpenCV 2.3.0, which uses the legacy IplImage C API (not the modern C++ Mat API). This is an older version of OpenCV.

**Include Paths** (Win32 Debug only):
- `C:\OpenCV-2.3.0\include`

**Library Paths** (Win32 Debug only):
- `C:\OpenCV-2.3.0\lib`

**Link Libraries** (Win32 Debug):
- `opencv_core230.lib`
- `opencv_highgui230.lib`
- `opencv_imgproc230.lib`

If OpenCV is not installed at the default path, update the include and library paths in the project properties (Debug|Win32 configuration).

## Architecture: Prokudin-Gorsky Color Restoration

### Project Goal
Reconstruct a color image from three grayscale channel images (Blue, Green, Red) stacked vertically in a single image file using SSD alignment.

**Input:** Single image with B/G/R channels stacked vertically (height = 3N)
**Output:** Aligned and merged color RGB image

### Core Algorithm (SSD-based Channel Alignment)

#### 1. Channel Separation
- Split input image into 3 equal-height regions (or detect boundaries via variance analysis)
- Extract Blue, Green, Red channel images

#### 2. SSD Alignment Search
**SSD (Sum of Squared Differences):** Measure pixel-level difference between channel pairs
```
SSD(offset_dy, offset_dx) = Σ(Green[y,x] - Channel[y+offset_dy, x+offset_dx])²
```

**Brute-force search:**
- Iterate over offset range: `dy, dx ∈ [-100, +100]` (20,401 iterations per channel)
- For each offset, compute SSD across entire channel region
- Track minimum error and corresponding offset
- Result: `(optimalDy, optimalDx)` for Blue and Red channels

#### 3. Channel Merging
Synthesize RGB image using detected offsets:
```
result[y][x] = (B=chB[y+bestDy_B][x+bestDx_B], 
                 G=chG[y][x], 
                 R=chR[y+bestDy_R][x+bestDx_R])
```
With boundary checks to handle out-of-bounds pixels (default to 0/black).

### Evolution of Approaches (See `Assignment2_Summary.md` for details)

| Approach | Method | Benefit | Status |
|----------|--------|---------|--------|
| **1** | Simple height/3 + brute-force SSD | Baseline working | ✅ Core |
| **2** | Variance-based boundary detection | Remove black borders | ✅ Integrated |
| **3** | Sobel edge preprocessing | Exposure-robust alignment | ❌ Abandoned |
| **4** | Pyramid 2-level search | 4-5x speedup | ❌ Reverted |
| **5** | asdf.cpp style refactoring | Code clarity, consistency | ✅ Applied |

### Important Architectural Notes
- Uses legacy OpenCV 2.3.0 IplImage API (`cvCreateImage`, `cvSet2D`, `cvLoadImage`, `cvGet2D`)
- Manual memory management with `malloc`/`free` (not C++ `new`/`delete`)
- SSD computation uses `val[0]` direct channel access (assumes grayscale channel images)
- Boundary checks prevent out-of-bounds pixel access; out-of-bounds = 0 (black)
- All image windows displayed with `cvShowImage()` require `cvWaitKey()` to keep open
- Hard-coded image path: `c:\MultiMedia\AS2\pg1.jpg` (update as needed)

### Known Issues
- **Offset range limits:** Fixed ±100 pixel range; may miss optimal alignment if offset > 100
- **Computation cost:** 20,401 iterations × image pixels per channel (significant for large images)
- **Boundary detection:** Variance-based method may fail on some image types; fallback to simple height/3
- **Memory management:** Ensure all allocated images freed with `cvReleaseImage()` to avoid leaks

## Development Notes

- **Excluded Files**: Most `.cpp` files are excluded from the Debug|Win32 build. Only the file with `ExcludedFromBuild=false` builds.
- **OpenCV Configuration**: Include/Library paths and dependencies currently only configured for **Debug|Win32**. Other configurations (Release|Win32, Debug|x64, Release|x64) need manual setup if used.
- **Debug Output**: Use `printf()` for debug output. Appears in Visual Studio Output window or console.
- **Window Management**: Image windows require `cvWaitKey()` to remain visible; use `cvReleaseImage()` to free allocated images.
- **Memory Management**: Allocate images with `cvCreateImage()`, free with `cvReleaseImage()`. No memory leak issues if properly paired.
- **Hard-Coded Paths**: Image path (`c:\MultiMedia\AS2\pg1.jpg`) is hard-coded in `main()`. Update when testing different images.
- **Pixel Access**: Use `cvGet2D(img, y, x)` to read pixel values; returns `CvScalar` with `val[0]` for intensity (assumes grayscale).

## Common Tasks

### Switch Active Build Target
1. Edit `multimedia.vcxproj`
2. Set desired file's `ExcludedFromBuild` to `false` (for Debug|Win32 configuration)
3. Set all other files' `ExcludedFromBuild` to `true`
4. Rebuild solution

**Example:** To switch to `assignment2_hint.cpp`:
```xml
<ClCompile Include="howtofindSSD.cpp">
  <ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">true</ExcludedFromBuild>
</ClCompile>
<ClCompile Include="assignment2_hint.cpp">
  <!-- No ExcludedFromBuild tag → defaults to false (included) -->
</ClCompile>
```

### Debug SSD Alignment
1. Add `printf()` to `alignB()` and `alignR()` to print offset and error values
2. Visualize aligned channels before merging (display with `cvShowImage()`)
3. Check boundary conditions: ensure offset + pixel coordinates stay within bounds
4. Compare against known good implementations (e.g., `asdf.cpp`)

### Test Different Images
1. Update hard-coded path in `main()`: `cvLoadImage("c:\\path\\to\\image.jpg")`
2. Ensure image format matches: Blue/Green/Red stacked vertically (3N height)
3. Optionally adjust offset search range based on expected channel misalignment

### Test Against Reference Implementations
Compare `howtofindSSD.cpp` results against:
- **`asdf.cpp`** — Simple baseline; validate core SSD logic
- **`assignment2_hint.cpp`** — Full pipeline with boundary detection; check result quality
- **`Assignment2_Summary.md`** — See which approach performed best

## Notes on Future Improvements

- Consider modernizing to OpenCV 4.x and C++ Mat API for production use
- Expand offset search range (currently ±100) if larger misalignments expected
- Implement pyramid/coarse-to-fine search for faster alignment on large images
- Add configurable boundary detection (variance-based vs. simple height/3)
- Make image path and offset range command-line arguments
- Add result image quality metrics (PSNR, SSIM) to validate alignment
