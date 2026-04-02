#include <opencv2/opencv.hpp>
#include <stdio.h>
#include <climits>

const int SEARCH_RANGE = 15;
const int MAX_BRIGHTNESS = 255;
const int RGB_CHANNELS = 3;
const int PROGRESS_INTERVAL = 50;
const int SMOOTH_WINDOW = 7;
const double VAR_THRESH_RATIO = 0.4;
const int MAX_RUNS = 100;

void calculateVarianceY(IplImage* img, double* varY);
void smoothVariance(double* src, double* dst, int size, int window);
void findVarianceYBoundaries(double* smoothed, int size, int* y1Out, int* y2Out);
void findChannelBoundaries(IplImage* src, int* topY, int* bottomY);
IplImage* splitChannel(IplImage* src, int yStart, int yEnd);
long long computeSSD(IplImage* ref, IplImage* src, int dy, int dx);
void findBestOffset(IplImage* ref, IplImage* src, const char* channelName,
                    IplImage* previewDst, int* bestDy, int* bestDx);
IplImage* createInitialMerge(IplImage* chB, IplImage* chG, IplImage* chR,
                             int dyB, int dxB, int dyR, int dxR);
IplImage* mergeChannels(IplImage* chB, IplImage* chG, IplImage* chR,
                        int dyB, int dxB, int dyR, int dxR);
void showImageFit(const char* windowName, IplImage* img, int maxWidth, int maxHeight);
int orchestration(IplImage* src);

void calculateVarianceY(IplImage* img, double* varY)
{
    int width = img->width;
    int height = img->height;

    for (int y = 0; y < height; y++) {
        double sum = 0.0;
        for (int x = 0; x < width; x++) {
            CvScalar c = cvGet2D(img, y, x);
            double brightness = (c.val[0] + c.val[1] + c.val[2]) / 3.0;
            sum += brightness;
        }
        double mean = sum / width;

        double sqSum = 0.0;
        for (int x = 0; x < width; x++) {
            CvScalar c = cvGet2D(img, y, x);
            double brightness = (c.val[0] + c.val[1] + c.val[2]) / 3.0;
            double diff = brightness - mean;
            sqSum += diff * diff;
        }
        varY[y] = sqSum / width;
    }
}

void smoothVariance(double* src, double* dst, int size, int window)
{
    int half = window / 2;
    for (int i = 0; i < size; i++) {
        int start = (i - half > 0) ? (i - half) : 0;
        int end = (i + half < size - 1) ? (i + half) : (size - 1);
        double sum = 0.0;
        int cnt = 0;
        for (int j = start; j <= end; j++) {
            sum += src[j];
            cnt++;
        }
        dst[i] = (cnt > 0) ? (sum / cnt) : src[i];
    }
}

void findVarianceYBoundaries(double* smoothed, int size, int* y1Out, int* y2Out)
{
    *y1Out = size / 3;
    *y2Out = size * 2 / 3;

    double total = 0.0;
    for (int i = 0; i < size; i++)
        total += smoothed[i];
    double mean = total / size;
    double threshold = mean * VAR_THRESH_RATIO;

    struct Run { int start, end, length; };
    Run runs[MAX_RUNS];
    int runCount = 0;

    int i = 0;
    while (i < size && runCount < MAX_RUNS) {
        if (smoothed[i] < threshold) {
            int start = i;
            while (i < size && smoothed[i] < threshold)
                i++;
            int end = i - 1;
            runs[runCount].start = start;
            runs[runCount].end = end;
            runs[runCount].length = end - start + 1;
            runCount++;
        }
        else {
            i++;
        }
    }

    if (runCount < 2) {
        printf("[Variance] Detected %d runs. Using fallback: topY=%d, bottomY=%d\n",
               runCount, *y1Out, *y2Out);
        return;
    }

    for (int a = 0; a < 2 && a < runCount - 1; a++) {
        int maxIdx = a;
        for (int b = a + 1; b < runCount; b++) {
            if (runs[b].length > runs[maxIdx].length)
                maxIdx = b;
        }
        if (maxIdx != a) {
            Run tmp = runs[a];
            runs[a] = runs[maxIdx];
            runs[maxIdx] = tmp;
        }
    }

    int centerA = (runs[0].start + runs[0].end) / 2;
    int centerB = (runs[1].start + runs[1].end) / 2;

    *y1Out = (centerA < centerB) ? centerA : centerB;
    *y2Out = (centerA < centerB) ? centerB : centerA;

    printf("[Variance] topY=%d, bottomY=%d\n", *y1Out, *y2Out);
}

void findChannelBoundaries(IplImage* src, int* topY, int* bottomY)
{
    printf("[Boundary] Analyzing image variance...\n");

    int height = src->height;
    double* varY = (double*)malloc(sizeof(double) * height);
    double* smoothed = (double*)malloc(sizeof(double) * height);

    calculateVarianceY(src, varY);
    smoothVariance(varY, smoothed, height, SMOOTH_WINDOW);
    findVarianceYBoundaries(smoothed, height, topY, bottomY);

    free(varY);
    free(smoothed);
}

IplImage* splitChannel(IplImage* src, int yStart, int yEnd)
{
    if (!src || yStart < 0 || yEnd > src->height || yStart >= yEnd)
        return NULL;

    int newH = yEnd - yStart;
    int newW = src->width;
    IplImage* dst = cvCreateImage(cvSize(newW, newH), src->depth, src->nChannels);

    for (int y = 0; y < newH; y++) {
        for (int x = 0; x < newW; x++) {
            CvScalar c = cvGet2D(src, y + yStart, x);
            cvSet2D(dst, y, x, c);
        }
    }
    return dst;
}

long long computeSSD(IplImage* ref, IplImage* src, int dy, int dx)
{
    int refH = ref->height, refW = ref->width;
    int srcH = src->height, srcW = src->width;

    long long ssd = 0;
    for (int y = 0; y < refH; y++) {
        int sy = y + dy;
        if (sy < 0 || sy >= srcH) continue;

        for (int x = 0; x < refW; x++) {
            int sx = x + dx;
            if (sx < 0 || sx >= srcW) continue;

            CvScalar cr = cvGet2D(ref, y, x);
            CvScalar cs = cvGet2D(src, sy, sx);

            double diff = cr.val[0] - cs.val[0];
            ssd += (long long)(diff * diff);
        }
    }
    return ssd;
}

void findBestOffset(IplImage* ref, IplImage* src, const char* channelName,
                    IplImage* previewDst, int* bestDy, int* bestDx)
{
    long long bestSSD = LLONG_MAX;
    *bestDy = 0;
    *bestDx = 0;

    int totalIterations = (2 * SEARCH_RANGE + 1) * (2 * SEARCH_RANGE + 1);
    int currentIter = 0;

    printf("\n[Align] Starting SSD search for %s channel (Range: +/-%d)...\n",
           channelName, SEARCH_RANGE);
    printf("[Align] Total iterations: %d\n", totalIterations);

    for (int dy = -SEARCH_RANGE; dy <= SEARCH_RANGE; dy++) {
        for (int dx = -SEARCH_RANGE; dx <= SEARCH_RANGE; dx++) {
            long long ssd = computeSSD(ref, src, dy, dx);

            if (ssd < bestSSD) {
                bestSSD = ssd;
                *bestDy = dy;
                *bestDx = dx;

                IplImage* tempMerge = createInitialMerge(src, ref, src, dy, dx, 0, 0);
                if (tempMerge && previewDst) {
                    for (int y = 0; y < previewDst->height && y < tempMerge->height; y++) {
                        for (int x = 0; x < previewDst->width && x < tempMerge->width; x++) {
                            CvScalar c = cvGet2D(tempMerge, y, x);
                            cvSet2D(previewDst, y, x, c);
                        }
                    }
                    cvShowImage(channelName, previewDst);
                    cvWaitKey(1);
                }
                if (tempMerge) cvReleaseImage(&tempMerge);
            }

            currentIter++;
            if (currentIter % PROGRESS_INTERVAL == 0) {
                int percent = (currentIter * 100) / totalIterations;
                printf("[Align] Progress: %d%% (%d/%d) - Current Best: dy=%d, dx=%d, SSD=%lld\n",
                    percent, currentIter, totalIterations, *bestDy, *bestDx, bestSSD);
            }
        }
    }
    printf("[Align] ===== FINAL RESULT for %s =====\n", channelName);
    printf("[Align] Best Offset: dy=%d, dx=%d (SSD=%lld)\n",
        *bestDy, *bestDx, bestSSD);
}

IplImage* createInitialMerge(IplImage* chB, IplImage* chG, IplImage* chR,
                             int dyB, int dxB, int dyR, int dxR)
{
    if (!chG) return NULL;

    int H = chG->height;
    int W = chG->width;
    IplImage* dst = cvCreateImage(cvSize(W, H), 8, RGB_CHANNELS);
    cvSet(dst, cvScalar(0, 0, 0));

    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            CvScalar g = cvGet2D(chG, y, x);
            double gVal = g.val[0];

            double bVal = 0.0;
            int by = y + dyB, bx = x + dxB;
            if (by >= 0 && by < chB->height && bx >= 0 && bx < chB->width) {
                CvScalar b = cvGet2D(chB, by, bx);
                bVal = b.val[0];
            }

            double rVal = 0.0;
            int ry = y + dyR, rx = x + dxR;
            if (ry >= 0 && ry < chR->height && rx >= 0 && rx < chR->width) {
                CvScalar r = cvGet2D(chR, ry, rx);
                rVal = r.val[0];
            }

            cvSet2D(dst, y, x, cvScalar(bVal, gVal, rVal));
        }
    }
    return dst;
}

IplImage* mergeChannels(IplImage* chB, IplImage* chG, IplImage* chR,
                        int dyB, int dxB, int dyR, int dxR)
{
    int H = chG->height;
    int W = chG->width;

    printf("\n[Merge] Creating final merged image with offsets:\n");
    printf("[Merge]   Blue: dy=%d, dx=%d\n", dyB, dxB);
    printf("[Merge]   Red:  dy=%d, dx=%d\n", dyR, dxR);

    IplImage* dst = cvCreateImage(cvSize(W, H), 8, RGB_CHANNELS);
    cvSet(dst, cvScalar(0, 0, 0));

    printf("[Merge] Processing pixels: ");
    for (int y = 0; y < H; y++) {
        if (y % (H / 10) == 0) {
            printf("%d%% ", (y * 100) / H);
            fflush(stdout);
        }

        for (int x = 0; x < W; x++) {
            // Green (Reference, no offset)
            CvScalar g = cvGet2D(chG, y, x);

            // Blue (Apply offset, check bounds)
            double bVal = 0.0;
            int by = y + dyB, bx = x + dxB;
            if (by >= 0 && by < chB->height && bx >= 0 && bx < chB->width) {
                CvScalar b = cvGet2D(chB, by, bx);
                bVal = b.val[0];
            }

            // Red (Apply offset, check bounds)
            double rVal = 0.0;
            int ry = y + dyR, rx = x + dxR;
            if (ry >= 0 && ry < chR->height && rx >= 0 && rx < chR->width) {
                CvScalar r = cvGet2D(chR, ry, rx);
                rVal = r.val[0];
            }

            // OpenCV BGR order: (B, G, R)
            cvSet2D(dst, y, x, cvScalar(bVal, g.val[0], rVal));
        }
    }
    printf("100%%\n");
    printf("[Merge] Image merge completed.\n");
    return dst;
}

void showImageFit(const char* windowName, IplImage* img,
    int maxWidth, int maxHeight)
{
    float scaleW = (float)maxWidth / img->width;
    float scaleH = (float)maxHeight / img->height;
    float scale = (scaleW < scaleH) ? scaleW : scaleH;

    if (scale < 1.0f) {
        IplImage* resized = cvCreateImage(
            cvSize((int)(img->width * scale), (int)(img->height * scale)),
            img->depth, img->nChannels);
        cvResize(img, resized, CV_INTER_LINEAR);
        cvShowImage(windowName, resized);
        cvResizeWindow(windowName, resized->width, resized->height);
        cvReleaseImage(&resized);
    }
    else {
        cvShowImage(windowName, img);
        cvResizeWindow(windowName, img->width, img->height);
    }
}

int orchestration(IplImage* src)
{
    if (!src) {
        printf("[ERROR] Source image is NULL.\n");
        return -1;
    }

    CvSize size = cvGetSize(src);
    printf("\n========== OPTIMIZED CHANNEL ALIGNMENT PIPELINE ==========\n\n");

    printf("[STEP 1] Image Information\n");
    printf("  Dimensions: %d x %d pixels\n", size.width, size.height);

    printf("\n[STEP 2] Detect Channel Boundaries using Variance\n");
    int topY, bottomY;
    findChannelBoundaries(src, &topY, &bottomY);

    if (topY <= 0 || bottomY <= topY || bottomY >= size.height) {
        printf("[INFO] Invalid boundaries. Using fallback (height/3).\n");
        topY = size.height / 3;
        bottomY = size.height * 2 / 3;
    }
    printf("  Blue:  [0, %d)\n", topY);
    printf("  Green: [%d, %d)\n", topY, bottomY);
    printf("  Red:   [%d, %d)\n", bottomY, size.height);

    IplImage* chB = splitChannel(src, 0, topY);
    IplImage* chG = splitChannel(src, topY, bottomY);
    IplImage* chR = splitChannel(src, bottomY, size.height);

    if (!chB || !chG || !chR) {
        printf("[ERROR] Channel splitting failed.\n");
        if (chB) cvReleaseImage(&chB);
        if (chG) cvReleaseImage(&chG);
        if (chR) cvReleaseImage(&chR);
        return -1;
    }
    printf("  OK: Blue channel split\n");
    printf("  OK: Green channel split\n");
    printf("  OK: Red channel split\n");

    printf("\n[STEP 3] Display Split Channels\n");
    showImageFit("Blue Channel", chB, 800, 600);
    showImageFit("Green Channel (Reference)", chG, 800, 600);
    showImageFit("Red Channel", chR, 800, 600);

    printf("\n[STEP 4] SSD-based Channel Alignment\n");
    printf("  Using Green channel as reference (no offset)\n");

    IplImage* previewBlue = cvCreateImage(cvSize(chB->width, chB->height), 8, RGB_CHANNELS);
    IplImage* previewRed = cvCreateImage(cvSize(chR->width, chR->height), 8, RGB_CHANNELS);
    cvSet(previewBlue, cvScalar(0, 0, 0));
    cvSet(previewRed, cvScalar(0, 0, 0));

    int bestDy_B = 0, bestDx_B = 0;
    int bestDy_R = 0, bestDx_R = 0;

    findBestOffset(chG, chB, "Blue Channel (Real-time)", previewBlue, &bestDy_B, &bestDx_B);
    findBestOffset(chG, chR, "Red Channel (Real-time)", previewRed, &bestDy_R, &bestDx_R);

    cvReleaseImage(&previewBlue);
    cvReleaseImage(&previewRed);

    printf("\n[STEP 5] Final Synthesis\n");
    IplImage* result = mergeChannels(chB, chG, chR, bestDy_B, bestDx_B, bestDy_R, bestDx_R);

    if (result) {
        printf("[STEP 6] Display Final Result\n");
        cvShowImage("Aligned and Merged Result", result);
        printf("  OK: Result image displayed.\n");
        cvReleaseImage(&result);
    }

    printf("\n[STEP 7] Cleanup\n");
    cvReleaseImage(&chB);
    cvReleaseImage(&chG);
    cvReleaseImage(&chR);
    printf("  OK: All resources released.\n");

    printf("\n========== PIPELINE COMPLETE ==========\n\n");
    return 0;
}

int main()
{
    printf("\n");
    printf("========== PROKUDIN-GORSKY COLOR RESTORATION (v1.0) ==========\n");
    printf("SSD-based Channel Alignment and RGB Merge\n");
    printf("============================================================\n\n");

    printf("[LOAD] Reading image from: c:\\MultiMedia\\AS2\\pg1.jpg\n");

    IplImage* src = cvLoadImage("c:\\MultiMedia\\AS2\\pg1.jpg");
    if (!src) {
        printf("[ERROR] Could not load image.\n");
        printf("[ERROR] Path: c:\\MultiMedia\\AS2\\pg1.jpg\n");
        printf("[ERROR] Please verify the file exists and is readable.\n");
        return -1;
    }

    printf("[LOAD] OK: Image loaded successfully.\n");

    int result = orchestration(src);

    cvReleaseImage(&src);

    printf("[WAIT] Press any key in the image window to exit...\n");
    cvWaitKey(0);

    return result;
}
