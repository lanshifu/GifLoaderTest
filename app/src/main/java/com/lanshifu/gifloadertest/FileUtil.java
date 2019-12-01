package com.lanshifu.gifloadertest;

import android.content.Context;
import android.util.Log;

import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class FileUtil {

    private static final String TAG = "FileUtil";

    /**
     * 拷贝文件到某文件夹
     *
     * @param source      源文件
     * @param destination 目标文件夹
     * @return 拷贝成功则返回true, 否则返回false
     */
    public static boolean copyFile(File source, File destination) {
        if (!source.exists() || !source.isFile())
            return false;
        if (!destination.exists() && !destination.mkdirs())
            return false;
        if (!destination.exists() || !destination.isDirectory())
            return false;

        FileInputStream fis = null;
        FileOutputStream fos = null;
        try {
            fis = new FileInputStream(source);
            fos = new FileOutputStream(new File(destination, source.getName()));
            byte[] buf = new byte[1024];
            int len;
            while ((len = fis.read(buf)) != -1) {
                fos.write(buf, 0, len);
            }
            return true;
        } catch (FileNotFoundException e) {
            Log.e(TAG, e.toString());
        } catch (Exception e) {
            Log.e(TAG, e.toString());
        } finally {
            closeQuietly(fis);
            closeQuietly(fos);
        }
        return false;
    }

    /**
     * 复制asset里的文件到指定路径
     *
     * @param context  上下文
     * @param fileName 原文件名称
     * @param path     copy到指定的路径[路径包括文件名称]
     * @return 是否copy成功
     */
    public static boolean copyAssetFileToPath(Context context, String fileName, String path) {
        InputStream in = null;
        FileOutputStream out = null;
        try {
            File file = new File(path);
            if (!file.createNewFile()) {
                return false;
            }
            in = context.getAssets().open(fileName);
            out = new FileOutputStream(file);
            byte[] buf = new byte[1024];
            int len = 0;
            while ((len = in.read(buf)) != -1) {
                out.write(buf, 0, len);
            }
            return new File(path).exists();
        } catch (IOException e) {
            Log.e(TAG, e.toString());
            return false;
        } finally {
            closeQuietly(in);
            closeQuietly(out);
        }
    }


    public static void closeQuietly(Closeable c) {
        if (c != null) {
            try {
                c.close();
            } catch (IOException e) {
                Log.i(TAG, e.toString());
            }
        }
    }
}
