package com.lanshifu.gifloadertest;

import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.content.Context;
import android.graphics.Bitmap;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.os.FileUtils;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.View;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import com.bumptech.glide.Glide;

import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;

import dalvik.system.DexClassLoader;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private static final String TAG = "MainActivity-gif";
    private Bitmap bitmap;
    private GifHandler gifHandler;
    private ImageView iv_gif;


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Log.d(TAG, "java.library.path’:" + System.getProperty("java.library.path"));

        iv_gif = findViewById(R.id.iv_gif);
        findViewById(R.id.mBitLoadGif).setOnClickListener(this);
        findViewById(R.id.mBtnGlide).setOnClickListener(this);
        findViewById(R.id.mBtnLoad).setOnClickListener(this);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            requestPermissions(new String[]{Manifest.permission.READ_EXTERNAL_STORAGE}, 666);
        }

    }

    private void loadBitmap() {

        File file = new File(Environment.getExternalStorageDirectory(), "test.gif");
        //1 加载gif
        gifHandler = GifHandler.load(file.getAbsolutePath());
        //2.得到gif的宽高
        int width = gifHandler.getWidth(gifHandler.getNativeGifFile());
        int height = gifHandler.getHeight(gifHandler.getNativeGifFile());
        //3.根据gif宽高创建一个Bitmap
        bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        // 4.处理这个Bitmap，每一帧处理完都会回调run方法
        gifHandler.updateBitmap(gifHandler.getNativeGifFile(), bitmap, new Runnable() {
            @Override
            public void run() {
                //在子线程回调，需要切换到主线程操作UI
                runOnUiThread(new Runnable() {
                    @Override
                    public void run() {
                        iv_gif.setImageBitmap(bitmap);
                    }
                });
            }
        });


    }


    @Override
    public void onClick(View v) {
        switch (v.getId()){
            case R.id.mBitLoadGif:
                loadBitmap();
                break;

            case R.id.mBtnGlide:
                File file = new File(Environment.getExternalStorageDirectory(), "test.gif");
                Glide.with(MainActivity.this)
                        .load(file)
                        .into(iv_gif);
                break;

            case R.id.mBtnLoad:
                String soPath = Environment.getExternalStorageDirectory().toString() + "/libtemp.so";
                //模拟下载到指定目录
                File fromFile = new File(soPath);
                if (!fromFile.exists()){
                    boolean copyAssetFileToPath = FileUtil.copyAssetFileToPath(MainActivity.this, "libtemp.so", soPath);
                    if (!copyAssetFileToPath){
                        Toast.makeText(MainActivity.this,"拷贝到sdk卡失败",Toast.LENGTH_SHORT).show();
                        return;
                    }
                }

                fromFile = new File(soPath);
                if (!fromFile.exists()) {
                    Toast.makeText(MainActivity.this,"so不存在",Toast.LENGTH_SHORT).show();
                    return;
                }

                File libFile = MainActivity.this.getDir("libs", Context.MODE_PRIVATE);
                String targetDir = libFile.getAbsolutePath() + "/libtemp.so";
                //将下载下来的so拷贝到私有目录：/data/user/0/包名/app_libs/
                FileUtil.copyFile(fromFile, libFile);
                //加载so，传绝对路径
                System.load(targetDir);
                Toast.makeText(MainActivity.this,"动态加载so成功",Toast.LENGTH_SHORT).show();
                break;


        }
    }
}
