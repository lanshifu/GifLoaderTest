#include <jni.h>
#include <string>
#include <android/bitmap.h>
#include <malloc.h>
#include "log.h"
#include <jni.h>
#include <string>
#include <malloc.h>
#include <string.h>
#include "log.h"
#include "giflib/gif_lib.h"
#include "PthreadSleep.h"

#define DATA_OFFSET 3

#define UNSIGNED_LITTLE_ENDIAN(lo, hi)    ((lo) | ((hi) << 8))
#define ERROR_CODE 5000
#define  MAKE_COLOR_ABGR(r, g, b) ((0xff) << 24 ) | ((b) << 16 ) | ((g) << 8 ) | ((r) & 0xff)

using namespace std;

//跳出循环的条件
bool isDestroy = false;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("JNI_OnLoad");
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) == JNI_OK) {
        return JNI_VERSION_1_6;
    } else if (vm->GetEnv((void **) &env, JNI_VERSION_1_4) == JNI_OK) {
        return JNI_VERSION_1_4;
    }
    return JNI_ERR;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_com_lanshifu_gifloadertest_GifHandler_loadGif(JNIEnv *env, jclass clazz, jstring path) {

    const char *filePath = env->GetStringUTFChars(path, 0);

    int err;
    // 1.调用源码api里方法，打开gif，返回GifFileType实体
    GifFileType *GifFile = DGifOpenFileName(filePath, &err);

    LOGD("filePath = %s", filePath);
    LOGD("loadGif,SWidth = %d", GifFile->SWidth);
    LOGD("loadGif,SHeight = %d", GifFile->SHeight);
    return (long long) GifFile;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_lanshifu_gifloadertest_GifHandler_getWidth(JNIEnv *env, jclass clazz,
                                                    jlong nativeGifFile) {
    // 获取gif 宽
    GifFileType *GifFile = (GifFileType *) nativeGifFile;
    return GifFile->SWidth;
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_lanshifu_gifloadertest_GifHandler_getHeight(JNIEnv *env, jclass clazz,
                                                     jlong nativeGifFile) {
    // 获取gif 高
    GifFileType *GifFile = (GifFileType *) nativeGifFile;
    return GifFile->SHeight;
}


uint32_t gifColorToColorARGB(const GifColorType &color) {
    return (uint32_t) (MAKE_COLOR_ABGR(color.Red, color.Green, color.Blue));
}


void setColorARGB(uint32_t *sPixels, int imageIndex, ColorMapObject *colorMap,
                  GifByteType colorIndex, int transparentColorIndex) {

    if (imageIndex > 0 && colorIndex == transparentColorIndex) {
        return;
    }
    if (colorIndex != transparentColorIndex || transparentColorIndex == NO_TRANSPARENT_COLOR) {
        *sPixels = gifColorToColorARGB(colorMap->Colors[colorIndex]);
    } else {
        *sPixels = 0;
    }

}


//填充Bitmap
void drawBitmap(JNIEnv *env, jobject bitmap, const GifFileType *GifFile, GifRowType *ScreenBuffer,
                int bitmapWidth, ColorMapObject *ColorMap, int imageIndex,
                SavedImage *pSavedImage, int transparentColorIndex) {

    //锁定像素以确保像素的内存不会被移动，一副图片是二维数组
    void *pixels;
    AndroidBitmap_lockPixels(env, bitmap, &pixels);
    //拿到像素地址
    uint32_t *sPixels = (uint32_t *) pixels;

    //数据会偏移3，这个是固定的
    int dataOffset = sizeof(int32_t) * DATA_OFFSET;
    int dH = bitmapWidth * GifFile->Image.Top;
    GifByteType colorIndex;
    //从左到右，一层一层设置Bitmap像素
    for (int h = GifFile->Image.Top; h < GifFile->Image.Height; h++) {
        for (int w = GifFile->Image.Left; w < GifFile->Image.Width; w++) {
            //像素点下标，给一个像素点设置ARGB
            colorIndex = (GifByteType) ScreenBuffer[h][w];

            //sPixels[dH + w] Bitmap像素地址，通过遍历给每个像素点设置argb，Bitmap就有颜色了
            setColorARGB(&sPixels[dH + w],
                         imageIndex,
                         ColorMap,
                         colorIndex,
                         transparentColorIndex);

            //将颜色下标保存，循环播放的时候需要知道这个下标，直接取出来就可以
            pSavedImage->RasterBits[dataOffset++] = colorIndex;
        }

        //遍历下一层
        dH += bitmapWidth;
    }

    AndroidBitmap_unlockPixels(env, bitmap);
}


extern "C"
JNIEXPORT jint JNICALL
Java_com_lanshifu_gifloadertest_GifHandler_updateBitamap(JNIEnv *env, jclass clazz,
                                                         jlong nativeGifFile,
                                                         jobject bitmap, jobject runnable) {

    isDestroy = false;

    //强转gif结构体
    GifFileType *GifFile = (GifFileType *) nativeGifFile;

    //giflib的一些变量
    GifRowType *ScreenBuffer;
    SavedImage *pSavedImage;
    GifRecordType RecordType;
    GifByteType *Extension;
    GifByteType *GifExtension;
    ColorMapObject *ColorMap;

    int gifWidgh = GifFile->SWidth;
    int gifHeight = GifFile->SHeight;
    int transparentColorIndex = 0;
    int disposalMode = DISPOSAL_UNSPECIFIED;
    int bitmapWidth;
    int32_t *user_image_data;


    int Row, Col, Width, Height, ExtCode;
    int InterlacedOffset[] = {0, 4, 2, 1}; /* The way Interlaced image should. ：隔行扫描 */
    int InterlacedJumps[] = {8, 8, 4, 2};    /* be read - offsets and jumps... */

    //获取bitmapWidth，正常情况下 bitmapWidth == gifWidgh
    AndroidBitmapInfo bitmapInfo;
    AndroidBitmap_getInfo(env, bitmap, &bitmapInfo);
    bitmapWidth = bitmapInfo.stride / 4;

    //线程睡眠相关
    PthreadSleep threadSleep;
    int32_t delayTime = 0;

    //Runnable 的run方法id
    jclass runClass = env->GetObjectClass(runnable);
    jmethodID runMethod = env->GetMethodID(runClass, "run", "()V");

    //所有的gif图像共享一个屏幕（Screen），这个屏幕和我们的电脑屏幕不同，只是一个逻辑概念。所有的图像都会绘制到屏幕上面。
    //首先我们需要给屏幕分配内存：
    ScreenBuffer = (GifRowType *) malloc(gifHeight * sizeof(GifRowType));
    if (ScreenBuffer == NULL) {
        LOGE("ScreenBuffer malloc error");
        goto end;
    }

    //一行像素占用的内存大小
    size_t rowSize;
    rowSize = gifWidgh * sizeof(GifPixelType);
    ScreenBuffer[0] = (GifRowType) malloc(rowSize);

    /***** 给 ScreenBuffer 设置背景颜色为gif背景*/
    //设置第一行背景颜色
    for (int i = 0; i < gifWidgh; i++) {
        ScreenBuffer[0][i] = (GifPixelType) GifFile->SBackGroundColor;
    }
    //其它行拷贝第一行
    for (int i = 1; i < gifHeight; i++) {
        if ((ScreenBuffer[i] = (GifRowType) malloc(rowSize)) == NULL) {
            LOGE("Failed to allocate memory required, aborted.");
            goto end;
        }
        memcpy(ScreenBuffer[i], ScreenBuffer[0], rowSize);
    }


    /***** 循环解析gif数据，并根据不同的类型进行不同的处理*/
    do {
        //DGifGetRecordType函数用来获取下一块数据的类型
        if (DGifGetRecordType(GifFile, &RecordType) == GIF_ERROR) {
            LOGE("DGifGetRecordType Error = %d", GifFile->Error);
            goto end;

        }

        switch (RecordType) {
            //如果是图像数据块，需要绘制到 ScreenBuffer 中
            case IMAGE_DESC_RECORD_TYPE :
                // DGifGetImageDesc 函数是 获取gif的详细信息，例如 是否是隔行扫描，每个像素点的颜色信息等等
                if (DGifGetImageDesc(GifFile) == GIF_ERROR) {
                    LOGE("DGifGetImageDesc Error = %d", GifFile->Error);
                    return ERROR_CODE;
                }

                Row = GifFile->Image.Top; /* Image Position relative to Screen. */
                Col = GifFile->Image.Left;
                Width = GifFile->Image.Width;
                Height = GifFile->Image.Height;

                //正常情况下这个条件不成立
                if (GifFile->Image.Left + GifFile->Image.Width > GifFile->SWidth ||
                    GifFile->Image.Top + GifFile->Image.Height > GifFile->SHeight) {
                    LOGE("Image is not confined to screen dimension, aborted");
                    goto end;
                }

                //隔行扫描
                if (GifFile->Image.Interlace) {
                    //隔行扫描，要执行扫描4次才完整绘制完
                    for (int i = 0; i < 4; i++)
                        for (int j = Row + InterlacedOffset[i];
                             j < Row + Height; j += InterlacedJumps[i]) {
                            // 从GifFile 中获取一行数据，放到ScreenBuffer 中去
                            if (DGifGetLine(GifFile, &ScreenBuffer[j][Col], Width) == GIF_ERROR) {
                                LOGE("DGifGetLine Error = %d", GifFile->Error);
                                goto end;
                            }
                        }
                } else {
                    //没有隔行扫描，顺序一行一行来
                    for (int i = 0; i < Height; i++) {
                        if (DGifGetLine(GifFile, &ScreenBuffer[Row++][Col], Width) == GIF_ERROR) {
                            LOGE("DGifGetLine Error = %d", GifFile->Error);
                            goto end;
                        }
                    }
                }

                //扫描完成，ScreenBuffer 中每个像素点是什么颜色就确定好了，就差绘制到Bitmap上了
                ColorMap = (GifFile->Image.ColorMap
                            ? GifFile->Image.ColorMap
                            : GifFile->SColorMap);

                //SavedImage 表示当前这一帧图片信息，GifFile->ImageCount 会从1开始递增，表示当前解析到第几张图片
                //这里获取SavedImage主要是想保存一些数据到当前解析的这帧图片中，循环播放的时候可以直接取出来用
                pSavedImage = &GifFile->SavedImages[GifFile->ImageCount - 1];
                //RasterBits 字段分配内存
                pSavedImage->RasterBits = (unsigned char *) malloc(
                        sizeof(GifPixelType) * GifFile->Image.Width * GifFile->Image.Height +
                        sizeof(int32_t) * 2);
                //RasterBits 这块内存用来保存一些数据，延时、透明颜色下标等，循环播放的时候要用到
                user_image_data = (int32_t *) pSavedImage->RasterBits;
                user_image_data[0] = delayTime;
                user_image_data[1] = transparentColorIndex;
                user_image_data[2] = disposalMode;

                //睡眠，delayTime 表示帧间隔时间，是从另一个数据块计算出来的
                threadSleep.msleep(delayTime * 10.0);
                delayTime = 0;

                //将数据绘制到Bitmap上
                drawBitmap(env, bitmap, GifFile, ScreenBuffer, bitmapWidth, ColorMap,
                           GifFile->ImageCount - 1, pSavedImage, transparentColorIndex);

                //Bitmap绘制好了，回调runnable的run方法，Java层刷新ImageView即可看到新的一帧图片
                env->CallVoidMethod(runnable, runMethod);
                break;

                //额外信息块，获取帧之间间隔、透明颜色下标
            case EXTENSION_RECORD_TYPE:

                //获取额外数据，这个函数只会返回一个数据块，调用这个函数后要调用DGifGetExtensionNext，
                if (DGifGetExtension(GifFile, &ExtCode, &Extension) == GIF_ERROR) {
                    LOGE("DGifGetExtension");
                    goto end;
                }
                if (ExtCode == GRAPHICS_EXT_FUNC_CODE) {
                    if (Extension[0] != 4) {
                        LOGE("Extension[0] != 4");
                        goto end;
                    }
                    GifExtension = Extension + 1;

                    // 获取帧间隔，这些计算方法就先不追究了，必须要知道Gif格式，帧之间间隔单位是 10 ms
                    delayTime = UNSIGNED_LITTLE_ENDIAN(GifExtension[1], GifExtension[2]);
                    if (delayTime < 1) { // 如果没有时间，写个默认5
                        delayTime = 5;
                    }
                    if (GifExtension[0] & 0x01) {
                        //获取透明颜色下标，设置argb的时候要特殊处理
                        transparentColorIndex = (int) GifExtension[3];
                    } else {
                        transparentColorIndex = NO_TRANSPARENT_COLOR;
                    }
                    disposalMode = (GifExtension[0] >> 2) & 0x07;
                }
                while (Extension != NULL) {
                    //跳过其它块
                    if (DGifGetExtensionNext(GifFile, &Extension) == GIF_ERROR) {
                        LOGE("DGifGetExtensionNext error %d", GifFile->Error);
                        goto end;
                    }
                }
                break;

            case TERMINATE_RECORD_TYPE:
                LOGD("TERMINATE_RECORD_TYPE");
                break;

            default:
                break;

        }


    } while (RecordType != TERMINATE_RECORD_TYPE);


    LOGD("跳出 do while");

    //无限循环播放
    while (!isDestroy) {
        for (int t = 0; t < GifFile->ImageCount; t++) {

            SavedImage frame = GifFile->SavedImages[t];
            GifImageDesc frameInfo = frame.ImageDesc;
//            LOGI("GifFile Image %d at (%d, %d) [%dx%d]", t, frameInfo.Left, frameInfo.Top,
//                 frameInfo.Width, frameInfo.Height);
            ColorMap = (frameInfo.ColorMap
                        ? frameInfo.ColorMap
                        : GifFile->SColorMap);
            //

            user_image_data = (int32_t *) frame.RasterBits;
            delayTime = user_image_data[0];
            transparentColorIndex = user_image_data[1];
            disposalMode = user_image_data[2];
            if (delayTime < 1) { // 如果没有时间，写个默认5。。。
                delayTime = 5;
            }

            threadSleep.msleep(delayTime * 10);

            void *pixels;
            AndroidBitmap_lockPixels(env, bitmap, &pixels);
            uint32_t *sPixels = (uint32_t *) pixels;
//           LOGI("d_time: %d tColorIndex: %d", d_time, tColorIndex);
            //
            int pointPixelIdx = sizeof(int32_t) * DATA_OFFSET;
            int dH = bitmapWidth * frameInfo.Top;
            for (int h = frameInfo.Top; h < frameInfo.Top + frameInfo.Height; h++) {
                for (int w = frameInfo.Left; w < frameInfo.Left + frameInfo.Width; w++) {
                    setColorARGB(&sPixels[dH + w],
                                 t,
                                 ColorMap,
                                 frame.RasterBits[pointPixelIdx++], transparentColorIndex);
                }
                dH += bitmapWidth;
            }
            //
            AndroidBitmap_unlockPixels(env, bitmap);
            env->CallVoidMethod(runnable, runMethod);
        }
    }


    end:
    if (ScreenBuffer) {
        free(ScreenBuffer);
    }

    int Error;
    if (DGifCloseFile(GifFile, &Error) == GIF_ERROR) {
        LOGE("DGifCloseFile error  %d", Error);
    }
    GifFile = NULL;

    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_lanshifu_gifloadertest_GifHandler_destroy(JNIEnv *env, jclass clazz,
                                                   jlong native_gif_file) {
    // TODO: implement destroy()

    isDestroy = true;
}