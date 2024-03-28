#import "OPSQLite.h"
#if RCT_NEW_ARCH_ENABLED
#import <React/RCTBridge+Private.h>
#endif
#import <React/RCTUtils.h>
#import <ReactCommon/RCTTurboModule.h>
#import <jsi/jsi.h>
#import "../cpp/bindings.h"

@implementation OPSQLite

@synthesize bridge=_bridge;

RCT_EXPORT_MODULE()

- (void)setBridge:(RCTBridge *)bridge {
  _bridge = bridge;
}

+ (BOOL)requiresMainQueueSetup
{
  return YES;
}

- (NSDictionary *)constantsToExport {
    NSArray *libraryPaths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, true);
    NSString *libraryPath = [libraryPaths objectAtIndex:0];
    
    NSArray *documentPaths = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, true);
    NSString *documentPath = [documentPaths objectAtIndex:0];
    return @{
        @"IOS_DOCUMENT_PATH": documentPath,
        @"IOS_LIBRARY_PATH": libraryPath
    };
}

- (NSDictionary *)getConstants {
    return [self constantsToExport];
}


RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
    RCTCxxBridge *cxxBridge = (RCTCxxBridge *)_bridge;
    if (cxxBridge == nil) {
        return @false;
    }
    

    auto jsiRuntime = (facebook::jsi::Runtime *)cxxBridge.runtime;
    if (jsiRuntime == nil) {
        return @false;
    }
    
    auto &runtime = *jsiRuntime;
    auto callInvoker = _bridge.jsCallInvoker;
    
    // Get appGroupID value from Info.plist using key "AppGroup"
    NSString *appGroupID = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"OPSQLite_AppGroup"];
    NSString *documentPath;
    
    if (appGroupID != nil) {
        // Get the app groups container storage url
        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSURL *storeUrl = [fileManager containerURLForSecurityApplicationGroupIdentifier:appGroupID];
        
        if (storeUrl == nil) {
            NSLog(@"OP-SQLite: Invalid AppGroup ID provided (%@). Check the value of \"AppGroup\" in your Info.plist file", appGroupID);
            return @false;
        }
        
        documentPath = [storeUrl path];
    } else {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, true);
        documentPath = [paths objectAtIndex:0];
    }
    
    opsqlite::install(runtime, callInvoker, [documentPath UTF8String]);
    return @true;
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(moveAssetsDatabase:(NSString *)name extension:(NSString *)extension) {
    NSString *documentPath = [NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, true) objectAtIndex:0];
    NSString *sourcePath = [[NSBundle mainBundle] pathForResource:name ofType:extension];
    NSString *destinationPath = [documentPath stringByAppendingPathComponent:[NSString stringWithFormat:@"%@.%@", name, extension]];

    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:destinationPath]) {
        return @true;
    }

    NSError *error;
    [fileManager copyItemAtPath:sourcePath toPath:destinationPath error:&error];
    if (error) {
        return @false;
    }
    return @true;
}

#if RCT_NEW_ARCH_ENABLED
- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
  return std::make_shared<facebook::react::NativeOPSQLiteSpecJSI>(params);
}
#endif

- (void)invalidate {
    opsqlite::clearState();
}

@end
