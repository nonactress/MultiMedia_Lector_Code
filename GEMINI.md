# GEMINI.md - Multimedia Programming (C++ & OpenCV)

This project is a collection of image processing assignments and exercises using C++ and the legacy OpenCV 2.3.0 library. It focuses on automated image analysis, boundary detection, and RGB channel manipulation.

## Project Overview
- **Core Purpose**: Educational image processing tasks (e.g., detecting boundaries between vertically stacked color channels, brightness analysis).
- **Primary Stack**: C++, OpenCV 2.3.0 (Legacy `IplImage` C API).
- **IDE/Build System**: Visual Studio 2022 (MSVC v143), MSBuild.
- **Key Algorithms**: First and second-order derivatives for edge detection, standard deviation for variance-based boundary detection.

## Architecture & File Organization
The project is structured as a single Visual Studio project (`multimedia.vcxproj`) containing multiple source files, only one of which is typically active (included in build) at a time.

- **Assignments**: `DP_Assignment1.cpp`, `DP_Assignment2.cpp`, `DP_Assignment2_Clean.cpp`, etc.
- **Exercises**: `weekX-Y.cpp` files representing weekly course progression.
- **Documentation**: 
  - `multimedia/REPORT.md`: Detailed technical report on the boundary detection logic (Variance vs. Brightness).
  - `CLAUDE.md`: Guidance for Claude AI interactions.
- **Legacy Dependencies**: Uses OpenCV 2.3.0 located at `C:\OpenCV-2.3.0` by default.

## Building and Running

### Switching the Active Task
Since multiple files contain a `main()` function, you must exclude all but one from the build:
1. Open `multimedia.sln` in Visual Studio.
2. Right-click a `.cpp` file -> **Properties** -> **Excluded From Build**.
3. Set **Yes** for files to ignore, and **No** for the file you want to run.
4. Alternatively, edit `multimedia/multimedia.vcxproj` directly and update `<ExcludedFromBuild>`.

### Build Commands
- **Visual Studio**: `Ctrl + Shift + B`.
- **CLI (MSBuild)**:
  ```powershell
  msbuild multimedia.sln /p:Configuration=Debug /p:Platform=Win32
  ```

### Running
- The output executable is located at `Debug/multimedia.exe`.
- **Note**: Many files use hard-coded image paths (e.g., `C:\MultiMedia\AS2\pg1.jpg`). Ensure the directories and files exist or update the paths in `main()`.

## Development Conventions

### Coding Style
- **API**: Strictly uses the legacy OpenCV C API (`IplImage*`, `cvCreateImage`, `cvReleaseImage`). Avoid the modern `cv::Mat` API unless refactoring the entire project.
- **Memory Management**: Uses `malloc`/`free` for internal arrays and `cvReleaseImage` for OpenCV resources.
- **Debugging**: Uses `printf` for console logging and `cvShowImage` for visual verification.
- **Structure**: Often uses a `struct ImageAnalysis` to pass around image metadata and calculated arrays (averages, derivatives).

### Image Processing Pipeline (DP_Assignment2)
1. **Brightness Analysis**: `brightnessAvgX/Y` to get 1D brightness profiles.
2. **Derivative Analysis**: `findEdge` (1st diff) and `findSecondDiff` (2nd diff) to find transitions.
3. **Variance Analysis**: `stage_calculateVarianceY` to detect regions of high/low complexity (useful for boundaries between identical images).
4. **ROI Extraction**: `cvSetImageROI` and `cvCopy` for cropping.

## Known Issues & Limitations
- **Hard-coded Paths**: Most scripts expect images at specific local paths.
- **Legacy OpenCV**: Version 2.3.0 is very old; modern systems may need compatibility adjustments for include/lib paths.
- **Ambiguous Peaks**: As noted in `REPORT.md`, simple derivatives can struggle with gradual transitions; 2nd derivatives or thresholds are recommended.
