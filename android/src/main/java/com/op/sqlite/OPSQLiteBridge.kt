package com.op.sqlite

import com.facebook.react.bridge.ReactContext
import com.facebook.react.turbomodule.core.CallInvokerHolderImpl
import com.facebook.react.common.annotations.FrameworkAPI

@OptIn(FrameworkAPI::class)
class OPSQLiteBridge {
    private external fun installNativeJsi(
        jsContextNativePointer: Long,
        jsCallInvokerHolder: CallInvokerHolderImpl,
        docPath: String
    )
    private external fun clearStateNativeJsi()

    fun install(context: ReactContext) {
        val jsContextPointer = context.javaScriptContextHolder!!.get()
        val jsCallInvokerHolder =
            context.catalystInstance.jsCallInvokerHolder as CallInvokerHolderImpl
        // Trick to get the base database path
        val dbPath =
            context.getDatabasePath("defaultDatabase").absolutePath.replace("defaultDatabase", "")
        installNativeJsi(
            jsContextPointer,
            jsCallInvokerHolder,
            dbPath
        )
    }

    fun invalidate() {
        clearStateNativeJsi()
    }

    companion object {
        val instance = OPSQLiteBridge()
    }
}