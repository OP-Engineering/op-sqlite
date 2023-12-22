package com.op.sqlite

import com.facebook.react.TurboReactPackage
import com.facebook.react.bridge.NativeModule
import com.facebook.react.bridge.ReactApplicationContext
import com.facebook.react.module.model.ReactModuleInfoProvider
import com.facebook.react.module.model.ReactModuleInfo

class OPSQLitePackage : TurboReactPackage() {
    override fun getModule(name: String, reactContext: ReactApplicationContext): NativeModule? {
        if(name == OPSQLiteModule.NAME) {
            return OPSQLiteModule(reactContext)
        } else {
            return null
        }
    }

    override fun getReactModuleInfoProvider() = ReactModuleInfoProvider {
        mapOf(
                 OPSQLiteModule.NAME to ReactModuleInfo(
                     OPSQLiteModule.NAME,
                     OPSQLiteModule.NAME,
                       false, // canOverrideExistingModule
                       false, // needsEagerInit
                       true, // hasConstants
                       false, // isCxxModule
                       true // isTurboModule
                             )
                      )
    }
}