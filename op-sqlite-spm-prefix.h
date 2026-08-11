// -include'd by Package.swift for every Objective-C++ source file. CocoaPods
// auto-generates a "<Target>-prefix.pch" that imports Foundation/UIKit into
// every ObjC translation unit; SwiftPM has no prefix-header mechanism, so
// this reproduces that ambient import for the .mm sources that rely on it.
#ifdef __OBJC__
#import <Foundation/Foundation.h>
#if __has_include(<UIKit/UIKit.h>)
#import <UIKit/UIKit.h>
#endif
#endif
