package dev.deusex.questvr;

import android.os.Bundle;
import android.util.Log;
import java.io.File;

public final class MainActivity extends android.app.NativeActivity {
    private static final String TAG = "DeusExQuest";

    static {
        System.loadLibrary("deusex_data_probe");
    }

    private static native boolean probeGameData(String gameRoot);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        File gameRoot = new File(getFilesDir(), "DeusEx");
        boolean valid = probeGameData(gameRoot.getAbsolutePath());
        Log.i(TAG, "UE1 on-device data probe result=" + valid + " root=" + gameRoot);
        super.onCreate(savedInstanceState);
    }
}
