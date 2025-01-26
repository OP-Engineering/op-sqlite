#import <React/RCTBridge.h>

@interface OPSQLite : NSObject <RCTBridgeModule>

@property(nonatomic, assign) BOOL setBridgeOnMainQueue;
+ (void)expoUpdatesWorkaround;
@end
