# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Visual Studio C++ project for multimedia programming, specifically focused on image processing with OpenCV. The repository contains multiple programming assignments and weekly exercises, likely for a computer science course or self-directed learning on image processing.

### Key Technologies
- **Language**: C++ (C++11/14, Visual Studio 2022 compatible)
- **Build System**: Visual Studio (MSVC v143) with MSBuild
- **Main Dependency**: OpenCV 2.3.0 (legacy IplImage API, not the modern Mat API)
- **Platforms**: Win32 (x86) and x64
- **Build Configurations**: Debug and Release

## File Organization

All source files are located in the `multimedia/` directory. The project structure reflects course progression:

- **Assignment Files**: `DP_Assignment1.cpp`, `DP_Assignment2.cpp` — formal assignments
- **Weekly Exercises**: `week2-1.cpp`, `week3-1.cpp`, `week3-2.cpp`, `week4-1.cpp`, `week4-2.cpp` — weekly programming exercises
- **Utilities**: `FileName.cpp`, `adjBriAndContrast.cpp` — utility programs (various image processing operations)
- **Project Files**: `multimedia.vcxproj` — Visual Studio project configuration

Currently, `DP_Assignment2.cpp` is the active build target (not excluded from build). All other `.cpp` files are excluded from the Debug|Win32 build configuration, allowing multiple implementations in the same project while building only the selected one.

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

## Architecture

### Image Processing Pipeline (DP_Assignment2)
The main assignment demonstrates image analysis with boundary detection and ROI extraction:

1. **Brightness Analysis**: Compute average brightness along X and Y axes using `brightnessAvgX()` and `brightnessAvgY()`
2. **Edge Detection**: Calculate first-order differences using `findEdge()` to detect brightness transitions
3. **Boundary Detection**: Scan diff arrays to locate ROI edges using `findBoundary()`:
   - **Minimum value** (negative peak) → left/top boundary
   - **Maximum value** (positive peak) → right/bottom boundary
4. **Second-Order Derivative**: Compute second-order differences with `findSecondDiff()` to locate peak changes (curvature analysis)
5. **ROI Visualization & Extraction**:
   - `visualizeBoundaryBox()` — overlay boundary rectangle on original image
   - `cropByBoundary()` — extract the detected ROI region
6. **RGB Channel Extraction**: Separate and recombine RGB channels with `extractRGBChannels()`

**Key Data Structure**: `ImageAnalysis` struct holds pre-allocated arrays and boundary coordinates:
- Arrays: `avgX`, `avgY` — brightness averages per row/column
- Arrays: `diffX`, `diffY` — first derivatives (brightness changes)
- Arrays: `diff2X`, `diff2Y` — second derivatives (curvature indicators)
- Boundaries: `leftX`, `rightX`, `topY`, `bottomY` — detected ROI edges (-1 if not detected)

### Important Architectural Notes
- Uses legacy OpenCV 2.3.0 IplImage API (`cvCreateImage`, `cvSet2D`, `cvLoadImage`, etc.)
- Manual memory management with `malloc`/`free` (not C++ `new`/`delete`)
- Boundary detection (`findBoundary()`) scans entire diff array to find global min/max positions
- ROI extraction uses `cvSetImageROI()` for efficient cropping without full image copying
- All image windows are displayed with `cvShowImage()` and require `cvWaitKey()` to remain open
- Hard-coded image path: `c:\MultiMedia\AS2\pg1.jpg` (update as needed)

## Development Notes

- **Excluded Files**: Most `.cpp` files are excluded from the Debug|Win32 build. To switch which file builds, edit `multimedia.vcxproj` and toggle the `ExcludedFromBuild` property for each file.
- **Debug Output**: The code uses `printf()` for debug output (e.g., edge detection thresholds).
- **Window Management**: Image display windows require `cvWaitKey()` to keep them visible; use `cvReleaseImage()` to free resources.
- **Memory Leaks**: Carefully manage OpenCV image allocations with `cvReleaseImage()` and struct allocations with `freeAnalysis()`.
- **Hard-Coded Paths**: Image file paths are hard-coded in `main()`. Update the path when testing with different images.

## Common Tasks

### Add a New Assignment File
1. Create a new `.cpp` file in the `multimedia/` directory
2. Add it to the project: right-click on the project → Add → Existing Item
3. Set other files to `ExcludedFromBuild=true` and leave the new file with `ExcludedFromBuild=false`
4. Rebuild

### Switch Active File for Building
Edit `multimedia.vcxproj` and update the `ExcludedFromBuild` properties:
```xml
<!-- Exclude all others -->
<ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">true</ExcludedFromBuild>
<!-- Enable your target -->
<ExcludedFromBuild Condition="'$(Configuration)|$(Platform)'=='Debug|Win32'">false</ExcludedFromBuild>
```

### Debug an Image Processing Operation
- Use the `showImageFit()` function to display intermediate results with auto-scaling
- Add `printf()` statements to trace values (e.g., edge positions, brightness values)
- The debug output appears in the Visual Studio Output window or console

## Notes on Future Improvements

The codebase is functional for learning image processing with OpenCV. When maintaining or extending:
- Consider modernizing to OpenCV 4.x and the C++ Mat API if starting from scratch
- The current IplImage API is suitable for educational purposes but is deprecated in modern OpenCV
- Add error handling for image loading failures (currently only checks `!src1`)
- The hard-coded image path should be made configurable (command-line argument or config file)
