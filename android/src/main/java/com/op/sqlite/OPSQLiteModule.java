package com.op.sqlite;

import androidx.annotation.NonNull;
import com.facebook.react.bridge.ReactApplicationContext;
import com.facebook.react.bridge.ReactContextBaseJavaModule;
import com.facebook.react.bridge.ReactMethod;
import java.util.HashMap;
import java.util.Map;

class OPSQLiteModule extends ReactContextBaseJavaModule {
  static {
    System.loadLibrary("op-sqlite");
  }
  
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
    ReactApplicationContext context = getReactApplicationContext();
    final String dbPath = context
                          .getDatabasePath("defaultDatabase")
                          .getAbsolutePath()
                          .replace("defaultDatabase", "");
    constants.put("ANDROID_DATABASE_PATH", dbPath);

    final String filesPath = context.getFilesDir().getAbsolutePath();
    constants.put("ANDROID_FILES_PATH", filesPath);

    final String externalFilesDir = context.getExternalFilesDir(null).getAbsolutePath();
    constants.put("ANDROID_EXTERNAL_FILES_PATH", externalFilesDir);

    return constants;
  }

  @ReactMethod(isBlockingSynchronousMethod = true)
  public boolean install() {
    try {
      OPSQLiteBridge.instance.install(getReactApplicationContext());
      return true;
    } catch (Exception exception) {
      return false;
    }
  }

  @Override
  public void onCatalystInstanceDestroy() {
    OPSQLiteBridge.instance.clearState();
  }
}