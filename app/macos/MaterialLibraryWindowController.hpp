#import <AppKit/AppKit.h>

typedef void (^PWOpenMaterialHandler)(NSURL* url);
typedef void (^PWRelocateMaterialHandler)(NSURL* oldURL, NSURL* newURL);
typedef BOOL (^PWCanRewriteMaterialHandler)(NSURL* url);

@interface MaterialLibraryWindowController : NSWindowController

- (instancetype)initWithOpenMaterialHandler:(PWOpenMaterialHandler)handler
                           relocationHandler:(PWRelocateMaterialHandler)relocationHandler
                           canRewriteHandler:(PWCanRewriteMaterialHandler)canRewriteHandler;
- (void)showMaterialLibrary;
- (void)noteMaterialSavedAtURL:(NSURL*)url;

@end
