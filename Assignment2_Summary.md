# Assignment2: Prokudin-Gorsky Color Restoration - Progress Summary

## Project Goal
Reconstruct a color image from three grayscale channel images (Blue, Green, Red) stacked vertically in a single image file using SSD (Sum of Squared Differences) alignment.

**Input:** Single image with B/G/R channels stacked vertically
**Output:** Aligned and merged color RGB image

---

## Approach 1: Simple Height/3 Channel Split + Brute-force SSD

### Method
1. **Channel Split:** Divide image height into 3 equal parts
   - Blue: rows [0, height/3)
   - Green: rows [height/3, 2*height/3)
   - Red: rows [2*height/3, height)

2. **SSD Alignment Search:**
   - Brute-force loop: dy, dx ∈ [-15, +15] (31×31 = 961 iterations per channel)
   - Compare Green (reference) vs Blue/Red with offset
   - Find (dy, dx) that minimizes SSD score

3. **Merge:** Synthesize RGB using best offsets
   - dst[y][x] = (B=chB[y+bestDy_B][x+bestDx_B], G=chG[y][x], R=chR[y+bestDy_R][x+bestDx_R])

### Implementation
- File: `assignment2_hint.cpp` (initial version)
- Functions: `splitChannel()`, `computeSSD()`, `findBestOffset()`, `mergeChannels()`
- Real-time preview: Updated preview window on each new best offset found

### Limitations
1. **Black borders:** Simple 1/3 split doesn't detect actual channel boundaries
   - Black border areas included in SSD calculation → noise + inaccuracy
   - SSD loop covers unnecessary pixels

2. **Computation cost:** 961 iterations × all pixels per channel
   - ~1억 SSD operations per channel
   - Real-time preview updates add overhead

3. **Result quality issues:**
   - Imprecise offset detection due to border noise
   - Final merged image shows color misalignment or artifacts

---

## Approach 2: Variance-based Boundary Detection

### Method
**Problem:** How to find actual channel boundaries instead of simple height/3?

**Solution:** Analyze variance (brightness spread) per row
- Channel boundaries = low variance regions (black borders/gaps)
- Content areas = high variance regions

### Algorithm
1. **calculateVarianceY():** For each row, compute variance of pixel brightness
   - Mean brightness = sum of all pixel intensities / width
   - Variance = sum((pixel - mean)²) / width
   
2. **smoothVariance():** Apply sliding window average (window=7)
   - Reduce noise in variance profile
   - Make valley detection more stable

3. **findVarianceYBoundaries():** Detect two major low-variance regions
   - Calculate threshold = mean_variance × 0.4
   - Find consecutive rows where variance < threshold
   - Identify runs (low-variance segments)
   - Select centers of 2 longest runs as topY and bottomY
   - Fallback: Use height/3, height*2/3 if detection fails

### Implementation
- Functions: `calculateVarianceY()`, `smoothVariance()`, `findVarianceYBoundaries()`, `findChannelBoundaries()`
- Integrated into `orchestration()` STEP 2

### Benefits
1. **Removes black borders:** Actual boundary detection → smaller ROI
2. **Reduced SSD search area:** Only compute within channel boundaries
3. **Better accuracy:** Cleaner input to SSD alignment

### Remaining Issues
- Result image still shows color misalignment
- Offset values (dy, dx) may be incorrect

---

## Approach 3: Edge-based SSD with Sobel Preprocessing

### Hypothesis
**Problem:** SSD using pixel brightness sensitive to exposure differences between channels

**Solution:** Use edge maps instead of raw brightness
- Extract structural features (edges) via Sobel operator
- Compare edges instead of brightness
- More robust to lighting variations

### Attempted Implementation
```cpp
IplImage* computeEdgeMap(IplImage* src) {
    1. Convert to grayscale
    2. Apply Sobel (X and Y derivatives)
    3. Compute magnitude = |sobelX| + |sobelY|
    4. Return edge map
}
```

Then use edge maps in SSD calculation instead of original images.

### Issues Encountered
1. **Sobel complexity:** IPL_DEPTH_16S (16-bit signed) pixel access complicated
2. **Manual edge detection:** Switched to simple difference-based edge detection
   - edgeX = |right - left|, edgeY = |bottom - top|
   - magnitude = edgeX + edgeY
3. **Preview image type mismatch:** Edge maps (1-channel) vs merge output (3-channel)

### Abandoned
- Complexity vs benefit trade-off unfavorable
- Reverted to simpler val[0] direct comparison (asdf.cpp style)

---

## Approach 4: Pyramid 2-Level SSD Search

### Hypothesis
**Problem:** 961 iterations too expensive; coarse search first, then refine

**Solution:** 2-stage hierarchical search
- Level 0: Search on 1/2 resolution image (cvPyrDown)
  - 961 iterations but only 1/4 pixels
  - Fast coarse search
  - Result × 2 → convert to original coordinates
  
- Level 1: Fine search on full resolution
  - Around Level 0 result ± 2 pixel range
  - 5×5 = 25 iterations on full image
  - Total: ~265 effective iterations vs 961

**Speedup:** ~4-5x faster

### Implementation
- Function: `findBestOffsetPyramid()`
- Uses `buildPyramidLevel()` wrapper around `cvPyrDown()`

### Issues
1. **cvPyrDown complexity:** Function availability/parameter issues
2. **Minimal speedup benefit:** Already using val[0] direct comparison
3. **Added complexity:** Multi-level logic harder to debug

### Status
- Implemented but abandoned
- Reverted to simple brute-force (Approach 1) for clarity

---

## Approach 5: Refactored with asdf.cpp Style

### Key Changes
1. **computeSSD simplification:**
   - Before: `br = (val[0] + val[1] + val[2]) / 3.0` (3-channel average)
   - After: `diff = val[0] - val[0]` (direct channel 0 comparison)
   - Matches asdf.cpp convention

2. **mergeChannels code clarity:**
   - Added comments: "Green (Reference, no offset)", "Blue (Apply offset, check bounds)"
   - Explicit BGR order: `cvScalar(bVal, g.val[0], rVal)`
   - Matches professional asdf.cpp style

### Rationale
- asdf.cpp uses val[0] directly → simpler, faster
- Assumes each channel image is grayscale (single intensity value)
- More consistent with legacy OpenCV C API

---

## Current State: assignment2_hint.cpp

### Architecture
```
main()
  → orchestration(src)
      → STEP 1: Image info
      → STEP 2: findChannelBoundaries() [Variance-based]
      → STEP 3: splitChannel() [3 channels from detected boundaries]
      → STEP 4: Display channels
      → STEP 5: findBestOffset() × 2 [Brute-force SSD with real-time preview]
      → STEP 6: mergeChannels() [Synthesize RGB with best offsets]
      → STEP 7: Display result
```

### Integrated Improvements
1. ✅ Variance-based boundary detection (Approach 2)
2. ✅ asdf.cpp style refactoring (Approach 5)
3. ✅ Real-time preview on best offset (Approach 1)
4. ❌ Edge preprocessing (Approach 3) - removed for simplicity
5. ❌ Pyramid search (Approach 4) - reverted to brute-force

---

## Known Issues & Limitations

### 1. Result Image Quality
- **Symptom:** Merged image shows color misalignment or incorrect colors
- **Possible Root Causes:**
  - topY/bottomY boundaries detected incorrectly
  - SSD offset values (dy, dx) wrong
  - Channel height mismatch during merge
  - Out-of-bounds pixel access → black regions

### 2. Boundary Detection Accuracy
- **Issue:** Variance analysis may fail on some image types
- **Mitigation:** Fallback to height/3 division
- **Uncertainty:** Whether variance-based topY/bottomY are correct

### 3. SSD Search Completeness
- **961 iterations:** Sufficient for ±15 pixel range (typical Prokudin-Gorsky offset ~10-20px)
- **Edge case:** If actual offset > 15, search will miss optimal alignment
- **Mitigation:** Could expand SEARCH_RANGE (trade speed for coverage)

### 4. Pixel Access Safety
- **Range checks:** Code checks bounds before reading pixels
- **Out-of-bounds → 0 (black):** May create black borders in result
- **Image size mismatch:** chB, chG, chR may have different heights after split

---

## Attempted Solutions Summary

| Approach | Method | Benefit | Issue | Status |
|----------|--------|---------|-------|--------|
| **1** | Simple height/3 + brute-force SSD | Baseline working | Black border noise | ✅ Core |
| **2** | Variance boundary detection | Remove borders | May fail on some images | ✅ Integrated |
| **3** | Sobel edge preprocessing | Exposure-robust SSD | Complex pixel access | ❌ Abandoned |
| **4** | Pyramid 2-level search | 4-5x speedup | Minimal benefit, added complexity | ❌ Reverted |
| **5** | asdf.cpp style refactoring | Code clarity, consistency | No functional change | ✅ Applied |

---

## Next Steps / Potential Solutions

### To Debug Current Issues:
1. **Print detected boundaries:** Check if topY, bottomY match actual channel positions
2. **Verify SSD offsets:** Print dy, dx values for Blue and Red channels
3. **Check image dimensions:** Print chB, chG, chR heights to confirm split
4. **Visualize intermediate steps:** Display variance profile, smoothed variance, boundaries

### To Improve Result Quality:
1. **Extend SEARCH_RANGE:** If dy/dx values at boundary ±15 → indicates search limit
2. **Use 3-channel brightness:** Switch from val[0] to 3-channel average for SSD
3. **Implement sub-pixel refinement:** After finding best integer offset, refine further
4. **Add cross-correlation:** Instead of SSD, try normalized cross-correlation (more robust)
5. **Revisit edge preprocessing:** For exposure-invariant alignment

### Architectural Improvements:
1. **Separate search and merge phases:** Debug by visualizing alignment before merge
2. **Intermediate result visualization:** Save/display offset-aligned channels separately
3. **Parameter tuning UI:** Make SEARCH_RANGE, VAR_THRESH_RATIO, SMOOTH_WINDOW adjustable
4. **Robustness:** Better error handling for edge cases (tiny channels, large offsets)

---

## Conclusion

**Progress:**
- Implemented working pipeline with variance-based boundary detection and SSD alignment
- Integrated real-time preview and progress reporting
- Refactored for code clarity and consistency

**Remaining Challenge:**
- Final merged image quality not optimal
- Root cause unclear: boundaries, SSD search, pixel access, or channel dimension mismatch

**Recommended Focus:**
- Debug intermediate outputs (boundaries, offsets, dimensions)
- Verify SSD values and offset ranges
- Consider simpler test images for validation
