#import <React/RCTBridgeModule.h>
#import <React/RCTInvalidating.h>

@interface OPSQLite : NSObject <RCTBridgeModule, RCTInvalidating>

@property(nonatomic, assign) BOOL setBridgeOnMainQueue;

@end
