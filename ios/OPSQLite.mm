#import "OPSQLite.h"
#import <React/RCTBridge+Private.h>
#import <React/RCTUtils.h>
#import <ReactCommon/RCTTurboModule.h>
#import <jsi/jsi.h>
#import "../cpp/bindings.h"

@implementation OPSQLite

RCT_EXPORT_MODULE(OPSQLite)

- (NSDictionary *)constantsToExport {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(NSLibraryDirectory, NSUserDomainMask, true);
    NSString *documentPath = [paths objectAtIndex:0];
    return @{ 
        @"IOS_DOCUMENT_PATH": @"New Event",
        @"IOS_LIBRARY_PATH": documentPath
    };
}

RCT_EXPORT_BLOCKING_SYNCHRONOUS_METHOD(install) {
    RCTBridge *bridge = [RCTBridge currentBridge];
    RCTCxxBridge *cxxBridge = (RCTCxxBridge *)bridge;
    if (cxxBridge == nil) {
        return @false;
    }
    
    using namespace facebook;
    
    auto jsiRuntime = (jsi::Runtime *)cxxBridge.runtime;
    if (jsiRuntime == nil) {
        return @false;
    }
    auto &runtime = *jsiRuntime;
    auto callInvoker = bridge.jsCallInvoker;
    
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
    
    NSLog(@"OP-SQLite initialized");
    return @true;
}

- (void)invalidate {
    opsqlite::clearState();
}

@end
