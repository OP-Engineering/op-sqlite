package com.op.sqlite

import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.bridge.ReactMethod
import com.facebook.react.module.annotations.ReactModule;
import java.io.File
import java.io.FileOutputStream
import java.io.InputStream
import java.io.OutputStream

@ReactModule(name = OPSQLiteModule.NAME)
internal class OPSQLiteModule(context: ReactApplicationContext?) :
    NativeOPSQLiteSpec(context) {
    override fun getName(): String {
        return NAME
    }

    override fun getTypedExportedConstants(): MutableMap<String, Any> {
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
        constants["IOS_DOCUMENT_PATH"] = ""
        constants["IOS_LIBRARY_PATH"] = ""
        return constants
    }

    @ReactMethod(isBlockingSynchronousMethod = true)
    override fun install(): Boolean {
        return try {
            OPSQLiteBridge.instance.install(reactApplicationContext)
            true
        } catch (exception: Exception) {
            false
        }
    }

    @ReactMethod(isBlockingSynchronousMethod = true)
    override fun moveAssetsDatabase(name: String, extension: String): Boolean {
        val context = reactApplicationContext
        val assetsManager = context.assets

        try {

//            val assets = assetsManager.list("");
            // Open the input stream for the asset file
            val inputStream: InputStream = assetsManager.open("custom/$name.$extension")

            // Create the output file in the documents directory
            val databasesFolder = context
                .getDatabasePath("defaultDatabase")
                .absolutePath
                .replace("defaultDatabase", "")

            val outputFile = File(databasesFolder, "$name.$extension")

            if (outputFile.exists()) {
                return true
            }

            // Open the output stream for the output file
            val outputStream: OutputStream = FileOutputStream(outputFile)

            // Copy the contents from the input stream to the output stream
            val buffer = ByteArray(1024)
            var length: Int
            while (inputStream.read(buffer).also { length = it } > 0) {
                outputStream.write(buffer, 0, length)
            }

            // Close the streams
            inputStream.close()
            outputStream.close()

            return true
        } catch (exception: Exception) {
            return false
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