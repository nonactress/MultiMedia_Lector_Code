# Image Processing: RGB Channel Boundary Detection Report

## Executive Summary

This report documents the development of an image processing pipeline designed to detect and separate RGB channels in a single image containing three vertically stacked identical photographs, each representing a single color channel. The implementation uses standard deviation analysis to identify boundaries between image regions.

---

## 1. Problem Statement

### 1.1 Initial Challenge
The task involved processing an image of dimensions (W × 3H) where:
- **Rows 0~H**: Red channel image
- **Rows H~2H**: Green channel image
- **Rows 2H~3H**: Blue channel image

These three images needed to be:
1. Individually extracted
2. Combined into a single RGB image
3. Boundaries automatically detected

### 1.2 Boundary Detection Difficulty

**Core Problem:** Simple min/max detection of pixel brightness differences is insufficient because:
- Pixel brightness naturally varies within an image
- Objects and textures within each channel create additional brightness transitions
- Cannot distinguish between **image boundaries** and **object features**

**Example:**
```
Background (low variance)  →  Image Content (high variance)  →  Next Image (sharp drop)
     var=5                        var=50                            var=5
                                  ↑
                         Hard to distinguish where
                      the actual boundary is located
```

### 1.3 Why Standard Deviation?

Standard deviation captures **variation within a region**, not just average brightness:
- Uniform backgrounds have **low standard deviation**
- Complex image content has **high standard deviation**
- Boundaries between regions show **sharp changes in standard deviation**

---

## 2. Solution Approach

### 2.1 Conceptual Design

The pipeline uses a **multi-stage analysis**:

```
Raw Image (Y-axis analysis)
    ↓
[STAGE 1] Calculate brightness average (avgY)
    ↓
[STAGE 1.5] Calculate standard deviation (stdY)
    ↓
[STAGE 2.5] Calculate stdY derivative (stddevDiff)
    ↓
[STAGE 3] Find top 12 peaks in stddevDiff
    ↓
Detected Boundaries (12 positions)
```

### 2.2 Key Metrics

**Standard Deviation (stdY):**
```
stdY[y] = sqrt( Σ(brightness[x,y] - avgY[y])² / width )
```
- Measures brightness variation in each row
- High in content-rich areas, low in uniform areas

**Standard Deviation Derivative (stddevDiff):**
```
stddevDiff[y] = stdY[y+1] - stdY[y]
```
- Detects **rapid changes** in variance
- Sharp peaks indicate boundaries

### 2.3 Why 12 Boundaries?

For 3 RGB channels:
- Each boundary requires both **top** and **bottom** edges
- Top 4 peaks (red group): R-channel boundaries
- Next 4 peaks (green group): G-channel boundaries
- Last 4 peaks (blue group): B-channel boundaries

---

## 3. Implementation Details

### 3.1 Core Functions

#### Stage 1.5: Calculate Standard Deviation
```cpp
void stage_calculateVarianceY(IplImage* img, int* avgY, int* stdY)
{
    // For each row y:
    // 1. Calculate mean brightness across all columns
    // 2. Compute sum of squared deviations
    // 3. Take square root: stdY[y] = sqrt(sumSquaredDeviations / width)
}
```

#### Stage 2.5: Calculate Derivative
```cpp
void stage_calculateStddevDiff(int* stdY, int* stddevDiff, int sizeY)
{
    // For each adjacent pair of rows:
    // stddevDiff[y] = stdY[y+1] - stdY[y]
}
```

#### Stage 3: Find Top 12 Boundaries
```cpp
void stage_findTop12Boundaries(int* stddevDiff, int sizeY, int* boundaries)
{
    // Step 1: Find positions with 12 largest absolute values
    // Step 2: Sort positions by Y-axis order (top to bottom)
    // Step 3: Filter with LIMITS_NEARBY (minimum distance constraint)
    // Step 4: Return final boundary positions
}
```

### 3.2 Key Constants

```cpp
const int LIMITS_NEARBY = 13;  // Minimum pixel distance between adjacent boundaries
const int RGB_CHANNELS = 3;
const int WINDOW_MAX_WIDTH = 1200;
const int WINDOW_MAX_HEIGHT = 800;
```

### 3.3 Visualization Functions

**visualize_stddevDiffGraphY():**
- Shows stddevDiff values on X-axis
- Center line (400): represents 0
- **Green (right)**: positive derivative (variance increasing)
- **Red (left)**: negative derivative (variance decreasing)
- Peaks indicate sharp transitions

**visualize_detected_boundaries():**
- Overlays detected boundaries on original image
- Color coding:
  - **Red**: Top 4 boundaries
  - **Green**: Next 4 boundaries
  - **Blue**: Last 4 boundaries

---

## 4. Current Limitations & Issues

### 4.1 Problem 1: Ambiguous Peaks

**Issue:** Multiple peaks appear near actual boundaries

**Root Cause Analysis:**
```
If boundary is gradual (not sharp spike):

stdY:        10 → 15 → 20 → 25 → 80 → 82 → ...
                                     ↑ actual edge

stddevDiff:  +5, +5, +5, +55, +2, -1, ...
            [small peaks] [big peak] [noise]

Multiple positive values before the sharp transition!
```

**Why LIMITS_NEARBY doesn't solve it:**
- LIMITS_NEARBY filters already-detected boundaries
- But the root problem is **gradual transitions create multiple spikes**
- The logic detects stddevDiff values, not final edge positions
- Multiple nearby values in stddevDiff → multiple detected peaks

### 4.2 Problem 2: Object Content vs Boundaries

**Issue:** High-variance regions within image content are indistinguishable from boundaries

**Example:**
```
Low Variance      |  High Variance  |  Low Variance    |  High Variance
(Plain bg)        |  (Object/Text)  |  (Gap between)   |  (Next image)
     var=5        |      var=50     |      var=5       |      var=50
     ↑                    ↑                  ↑                   ↑
   Can't tell          Can't tell        ACTUAL            Can't tell
   difference          difference       BOUNDARY          difference
```

Objects within image regions create variance spikes identical to boundary signatures.

---

## 5. Proposed Solutions

### 5.1 Solution A: Second Derivative of stddevDiff

**Concept:** Peak of peaks
```
stddevDiff²[y] = stddevDiff[y+1] - stddevDiff[y]

Gradual transition:  +5, +5, +5, +55, +2
                          0,  0, +50,  -53 ← Only ONE sharp peak

Sharp boundary:      -5, +100, -5
                         +105, -105 ← Single dominant peak
```

**Advantage:** Filters out gradual transitions, keeps sharp edges

### 5.2 Solution B: Threshold-Based Detection

**Concept:** Only consider "significant" changes
```cpp
const int STDDEV_THRESHOLD = 30;

if (abs(stddevDiff[y]) > STDDEV_THRESHOLD) {
    // Mark as boundary candidate
}
```

**Advantage:** Simple, filters noise, adjustable

### 5.3 Solution C: Adaptive Threshold

**Concept:** Use statistical properties of stddevDiff itself
```
mean = average(abs(stddevDiff))
threshold = mean + 1.5 * stdDev(abs(stddevDiff))

// Only peaks exceeding this adaptive threshold are boundaries
```

**Advantage:** Automatically adapts to image characteristics

---

## 6. Recommended Next Steps

### 6.1 Immediate Action
1. **Visualize stddevDiff graph** (DONE)
   - Examine pattern of peaks
   - Identify if gradient issue is present

2. **Analyze peak distribution**
   - Print stddevDiff values around detected boundaries
   - Check if multiple peaks cluster together

### 6.2 Implementation Priority

**Phase 1 (Quick Fix):**
- Implement Solution B (Threshold)
- Adjust STDDEV_THRESHOLD until clean detection

**Phase 2 (Robust Solution):**
- Implement Solution A (2nd Derivative)
- More robust to varying image types

**Phase 3 (Advanced):**
- Implement Solution C (Adaptive Threshold)
- Production-ready reliability

---

## 7. Code Structure Overview

### Current Architecture
```
[1] CONFIGURATION & CONSTANTS
    └─ LIMITS_NEARBY, RGB_CHANNELS, etc.

[2] DATA STRUCTURES
    └─ ImageAnalysis struct
       ├─ avgX, avgY (brightness)
       ├─ stdX, stdY (standard deviation)
       ├─ stddevDiff (derivatives)
       └─ boundaries[12] (detected positions)

[3] MEMORY MANAGEMENT
    └─ allocateAnalysis(), freeAnalysis()

[4] PROCESSING STAGES
    ├─ stage_calculateBrightnessY()
    ├─ stage_calculateVarianceY()
    ├─ stage_calculateStddevDiff()
    └─ stage_findTop12Boundaries()

[5] VISUALIZATION
    ├─ visualize_brightnessGraphY()
    ├─ visualize_stddevGraphY()
    ├─ visualize_stddevDiffGraphY()  ← NEW
    └─ visualize_detected_boundaries()

[6] MAIN PIPELINE
    └─ pipeline_analyzeImage() → orchestrates all stages

[7] ENTRY POINT
    └─ main()
```

---

## 8. Testing & Validation

### Current Approach
- Visual inspection via "Detected Boundaries" overlay
- Console output of detected positions and values
- Comparison with manual inspection

### Recommended Metrics
```
True Positive Rate:   Boundaries correctly detected / Total real boundaries
False Positive Rate:  Spurious detections / Total detections
Accuracy:            (TP + TN) / (TP + TN + FP + FN)
```

---

## 9. Conclusion

The implementation successfully:
✅ Calculates standard deviation per row
✅ Detects sharp transitions via derivative
✅ Identifies top 12 peaks
✅ Visualizes results with color coding

Remaining challenges:
⚠️ Gradual transitions create multiple peaks
⚠️ Cannot distinguish object content from boundaries
⚠️ Requires refinement for production use

Next phase focuses on **second derivative analysis** or **threshold-based filtering** to eliminate spurious detections.

---

**Document Generated:** 2026-03-30
**Implementation Status:** Phase 1 Complete, Phase 2 Pending
**Author:** Claude (with user collaboration)
