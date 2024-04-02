// #ifdef RCT_NEW_ARCH_ENABLED
// #import <OPSQLiteSpec/OPSQLiteSpec.h>
// #else
#import <React/RCTBridge.h>
// #endif

@interface OPSQLite : NSObject
// #ifdef RCT_NEW_ARCH_ENABLED
//                                    <NativeOPSQLiteSpec>
// #else
                                   <RCTBridgeModule>
// #endif

@property(nonatomic, assign) BOOL setBridgeOnMainQueue;

@end
