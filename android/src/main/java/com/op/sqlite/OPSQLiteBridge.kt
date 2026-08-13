package com.op.sqlite

import com.facebook.react.bridge.ReactContext
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl
import com.facebook.react.common.annotations.FrameworkAPI

@OptIn(FrameworkAPI::class)
class OPSQLiteBridge {
    // Returns an opaque handle to the native generation-liveness flag created
    // for this call. This object is a singleton shared by every JS runtime
    // generation, so it can't hold per-generation state itself -- the caller
    // (OPSQLiteModule, which IS recreated per generation) must hold onto the
    // handle and pass it back to invalidate().
    private external fun installNativeJsi(
        jsContextNativePointer: Long,
        jsCallInvokerHolder: CallInvokerHolderImpl,
        docPath: String
    ): Long
    private external fun clearStateNativeJsi(handle: Long)

    fun install(context: ReactContext): Long {
        val jsContextPointer = context.javaScriptContextHolder!!.get()
        val jsCallInvokerHolder =
            context.catalystInstance.jsCallInvokerHolder as CallInvokerHolderImpl
        // Trick to get the base database path
        val dbPath =
            context.getDatabasePath("defaultDatabase").absolutePath.replace("defaultDatabase", "")
        return installNativeJsi(
            jsContextPointer,
            jsCallInvokerHolder,
            dbPath
        )
    }

    fun invalidate(handle: Long) {
        clearStateNativeJsi(handle)
    }

    companion object {
        val instance = OPSQLiteBridge()
    }
}