package com.op.sqlite;

import androidx.annotation.NonNull;
import android.util.Log;

import com.facebook.jni.HybridData;
import com.facebook.jni.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;

class OPSQLiteModule extends ReactContextBaseJavaModule {
  public static final String NAME = "OPSQLite";
  
  public OPSQLiteModule(ReactApplicationContext context) {
    super(context);
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
  }

  @Override
  public Map<String, Object> getConstants() {
    final Map<String, Object> constants = new HashMap<>();
    final String dbPath = context
                          .getDatabasePath("defaultDatabase")
                          .getAbsolutePath()
                          .replace("defaultDatabase", "");

    constants.put("ANDROID_DATABASE_PATH", dbPath);

    final String externalPath = context
                                .getExternalPath();

    constant.put("ANDROID_EXTERNAL_FOLDER_PATH", externalPath);
    return constants;
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public boolean install() {
    try {
      System.loadLibrary("op-sqlite");
      OPSQLiteBridge.instance.install(getReactApplicationContext());
      return true;
    } catch (Exception exception) {
      Log.e(NAME, "Failed to install JSI Bindings!", exception);
      return false;
    }
  }

  @Override
  public void onCatalystInstanceDestroy() {
    try {
      OPSQLiteBridge.instance.clearState();
    } catch (Exception exception) {
      Log.e(NAME, "Failed to clear state!", exception);
    }
  }
}