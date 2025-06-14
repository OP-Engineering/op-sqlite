#import "OpSqlite.h"
#import "../cpp/bindings.h"
#import <ReactCommon/CallInvoker.h>
#import <ReactCommon/RCTTurboModuleWithJSIBindings.h>

using namespace facebook;

@implementation OPSQLite {
    bool _didInstall;
    std::weak_ptr<facebook::react::CallInvoker> _callInvoker;
}

RCT_EXPORT_MODULE()

- (NSDictionary *)constantsToExport {
    NSArray *libraryPaths = NSSearchPathForDirectoriesInDomains(
        NSLibraryDirectory, NSUserDomainMask, true);
    NSString *libraryPath = [libraryPaths objectAtIndex:0];

    NSArray *documentPaths = NSSearchPathForDirectoriesInDomains(
        NSDocumentDirectory, NSUserDomainMask, true);
    NSString *documentPath = [documentPaths objectAtIndex:0];
    return @{
        @"IOS_DOCUMENT_PATH" : documentPath,
        @"IOS_LIBRARY_PATH" : libraryPath
    };
}

- (NSDictionary *)getConstants {
    return [self constantsToExport];
}

// Taken from @mrousavy's libraries to initialize JSI bindings directly
- (void)installJSIBindingsWithRuntime:(jsi::Runtime &)runtime {
    auto callInvoker = _callInvoker.lock();
    if (callInvoker == nullptr) {
        throw std::runtime_error("CallInvoker is missing");
    }

    // Get appGroupID value from Info.plist using key "AppGroup"
    NSString *appGroupID =
        [[NSBundle mainBundle] objectForInfoDictionaryKey:@"OPSQLite_AppGroup"];
    NSString *documentPath;

    if (appGroupID != nil) {
        // Get the app groups container storage url
        NSFileManager *fileManager = [NSFileManager defaultManager];
        NSURL *storeUrl = [fileManager
            containerURLForSecurityApplicationGroupIdentifier:appGroupID];

        if (storeUrl == nil) {
            NSLog(@"OP-SQLite: Invalid AppGroup ID provided (%@). Check the "
                  @"value of "
                  @"\"AppGroup\" in your Info.plist file",
                  appGroupID);
            return;
        }

        documentPath = [storeUrl path];
    } else {
        NSArray *paths = NSSearchPathForDirectoriesInDomains(
            NSLibraryDirectory, NSUserDomainMask, true);
        documentPath = [paths objectAtIndex:0];
    }

    NSBundle *crsqlite_bundle =
        [NSBundle bundleWithIdentifier:@"io.vlcn.crsqlite"];
    NSString *crsqlite_path = [crsqlite_bundle pathForResource:@"crsqlite"
                                                        ofType:@""];
    NSBundle *libsqlitevec_bundle =
        [NSBundle bundleWithIdentifier:@"com.ospfranco.sqlitevec"];
    NSString *sqlite_vec_path =
        [libsqlitevec_bundle pathForResource:@"sqlitevec" ofType:@""];

    if (crsqlite_path == nil) {
        crsqlite_path = @"";
    }

    if (sqlite_vec_path == nil) {
        sqlite_vec_path = @"";
    }

    opsqlite::install(runtime, callInvoker, [documentPath UTF8String],
                      [crsqlite_path UTF8String], [sqlite_vec_path UTF8String]);
    _didInstall = true;
}

- (NSString *)install {
    if (_didInstall) {
        // installJSIBindingsWithRuntime ran successfully.
        return nil;
    } else {
        return @"JSI Bindings could not be installed!";
    }
}

- (nonnull NSString *)getDylibPath:(nonnull NSString *)bundleId
                      resourceName:(nonnull NSString *)resourceName {
    NSBundle *bundle = [NSBundle bundleWithIdentifier:bundleId];
    NSString *path = [bundle pathForResource:resourceName ofType:@""];
    return path;
}

- (void)moveAssetsDatabase:
            (JS::NativeOPSQLite::MoveAssetsDatabaseParams &)params
                   resolve:(nonnull RCTPromiseResolveBlock)resolve
                    reject:(nonnull RCTPromiseRejectBlock)reject {
    NSString *documentPath = [NSSearchPathForDirectoriesInDomains(
        NSLibraryDirectory, NSUserDomainMask, true) objectAtIndex:0];

    NSString *filename = params.filename();
    std::optional<bool> overwrite = params.overwrite();

    NSString *sourcePath = [[NSBundle mainBundle] pathForResource:filename
                                                           ofType:nil];

    NSString *destinationPath =
        [documentPath stringByAppendingPathComponent:filename];

    NSError *error;
    NSFileManager *fileManager = [NSFileManager defaultManager];
    if ([fileManager fileExistsAtPath:destinationPath]) {
        if (overwrite) {
            [fileManager removeItemAtPath:destinationPath error:&error];
            if (error) {
                NSLog(@"Error: %@", error);
                resolve(@false);
                return;
            }
        } else {
            resolve(@true);
            return;
        }
    }

    [fileManager copyItemAtPath:sourcePath toPath:destinationPath error:&error];
    if (error) {
        NSLog(@"Error: %@", error);
        resolve(@false);
        return;
    }
    resolve(@true);
}

- (void)invalidate {
    opsqlite::invalidate();
}

+ (void)expoUpdatesWorkaround {
    NSArray *paths = NSSearchPathForDirectoriesInDomains(
        NSLibraryDirectory, NSUserDomainMask, true);
    NSString *documentPath = [paths objectAtIndex:0];
    opsqlite::expoUpdatesWorkaround([documentPath UTF8String]);
}

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params {
    _callInvoker = params.jsInvoker;
    return std::make_shared<facebook::react::NativeOPSQLiteSpecJSI>(params);
}

@end
