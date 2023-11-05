package com.reactnativeOPSQLite;

import android.util.Log;

import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContext;
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl;

public class OPSQLiteBridge {
  private native void installNativeJsi(long jsContextNativePointer, CallInvokerHolderImpl jsCallInvokerHolder, String docPath);
  private native void clearStateNativeJsi();
  public static final OPSQLiteBridge instance = new OPSQLiteBridge();

  public void install(ReactContext context) {
      long jsContextPointer = context.getJavaScriptContextHolder().get();
      CallInvokerHolderImpl jsCallInvokerHolder = (CallInvokerHolderImpl)context.getCatalystInstance().getJSCallInvokerHolder();
      final String path = context.getFilesDir().getAbsolutePath();
      
      installNativeJsi(
        jsContextPointer,
        jsCallInvokerHolder,
        path
      );
  }

  public void clearState() {
    clearStateNativeJsi();
  }
}