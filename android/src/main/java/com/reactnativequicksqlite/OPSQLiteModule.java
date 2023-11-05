package com.reactnativeOPSQLite;

import androidx.annotation.NonNull;
import android.util.Log;

import com.facebook.jni.HybridData;
import com.facebook.jni.annotations.DoNotStrip;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;

class SequelModule extends ReactContextBaseJavaModule {
  public static final String NAME = "op-sqlite";
  
  public SequelModule(ReactApplicationContext context) {
    super(context);
  }

  @NonNull
  @Override
  public String getName() {
    return NAME;
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