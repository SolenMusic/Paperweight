#import <AppKit/AppKit.h>

#include <paperweight/material.hpp>

typedef void (^PWOpenPackedMaterialHandler)(paperweight::Material material);

@interface PackedLibraryWindowController : NSWindowController

@property(nonatomic, readonly) BOOL validLibrary;

- (instancetype)initWithURL:(NSURL*)url
                 openHandler:(PWOpenPackedMaterialHandler)openHandler;
- (void)showPackedLibrary;

@end
