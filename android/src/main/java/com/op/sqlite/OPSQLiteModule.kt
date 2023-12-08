package com.op.sqlite

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactContextBaseJavaModule
import com.facebook.react.bridge.ReactMethod

internal class OPSQLiteModule(context: ReactApplicationContext?) :
    ReactContextBaseJavaModule(context) {
    override fun getName(): String {
        return NAME
    }

    override fun getConstants(): Map<String, Any>? {
        val constants: MutableMap<String, Any> = HashMap()
        val context = reactApplicationContext
        val dbPath = context
            .getDatabasePath("defaultDatabase")
            .absolutePath
            .replace("defaultDatabase", "")
        constants["ANDROID_DATABASE_PATH"] = dbPath
        val filesPath = context.filesDir.absolutePath
        constants["ANDROID_FILES_PATH"] = filesPath
        val externalFilesDir = context.getExternalFilesDir(null)!!.absolutePath
        constants["ANDROID_EXTERNAL_FILES_PATH"] = externalFilesDir
        return constants
    }

    @ReactMethod(isBlockingSynchronousMethod = true)
    fun install(): Boolean {
        return try {
            OPSQLiteBridge.instance.install(reactApplicationContext)
            true
        } catch (exception: Exception) {
            false
        }
    }

    override fun onCatalystInstanceDestroy() {
        OPSQLiteBridge.instance.clearState()
    }

    companion object {
        init {
            System.loadLibrary("op-sqlite")
        }

        const val NAME = "OPSQLite"
    }
}