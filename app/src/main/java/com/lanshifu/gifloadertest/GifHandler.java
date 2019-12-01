package com.lanshifu.gifloadertest;

import android.graphics.Bitmap;
import android.os.Handler;
import android.os.HandlerThread;

public class GifHandler {

    private static final String TAG = "GifHandler";

    static {
        System.loadLibrary("native-lib");
    }

    static Handler mHandler;

    //native层GifFileType（giflib里面的）的地址
    private long nativeGifFile;

    public long getNativeGifFile() {
        return nativeGifFile;
    }

    private GifHandler(long nativeGifFile) {
        this.nativeGifFile = nativeGifFile;

        HandlerThread mHandlerThread = new HandlerThread("GifHandlerThread");
        mHandlerThread.start();
        mHandler = new Handler(mHandlerThread.getLooper());
    }

    public static GifHandler load(String path) {
        //先加载gif文件，获取native 层GifFile 的地址
        long nativeGifFile = loadGif(path);
        GifHandler gifHandler = new GifHandler(nativeGifFile);
        return gifHandler;

    }


    public static void updateBitmap(final long nativeGifFile, final Bitmap bitmap, final Runnable runnable){
        //在子线程更新Bitmap数据
        mHandler.post(new Runnable() {
            @Override
            public void run() {
                updateBitamap(nativeGifFile,bitmap,runnable);
            }
        });

    }

    // 定义一些native方法

    // 1.加载gif，返回 giflib中的 GifFileType对象地址,之后的操作都传这个GifFileType的地址过去
    public static native long loadGif(String gifPath);

    // 2. 获取gif宽高
    public static native int getWidth(long nativeGifFile);

    public static native int getHeight(long nativeGifFile);

    // 3.更新bitmap，更新成功就回调runnable
    public static native int updateBitamap(long nativeGifFile, Bitmap bitmap, Runnable runnable);

    public static native void destroy(long nativeGifFile);


}
