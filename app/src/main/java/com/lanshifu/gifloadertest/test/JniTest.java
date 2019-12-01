package com.lanshifu.gifloadertest.test;

public class JniTest {

    static {
        System.loadLibrary("temp");
    }

    public native int nativeAdd(int x, int y);

    public int add(int x, int y) {
        return x + y;
    }

    public static void main(String[] args) {
        JniTest jniTest = new JniTest();
        jniTest.nativeAdd(2012, 3);
        jniTest.add(2012, 3);

    }
}
