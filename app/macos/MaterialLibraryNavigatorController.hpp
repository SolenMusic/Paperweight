#import <AppKit/AppKit.h>

typedef void (^PWNavigatorOpenMaterialHandler)(NSURL* url);
typedef void (^PWNavigatorShowLibraryHandler)(void);

// A deliberately lightweight view of the current working library.  Each
// material editor owns one navigator, so a tab that is detached to another
// display remains useful without borrowing view state from the library window.
@interface MaterialLibraryNavigatorController : NSViewController

- (instancetype)initWithOpenMaterialHandler:(PWNavigatorOpenMaterialHandler)openHandler
                          showLibraryHandler:(PWNavigatorShowLibraryHandler)showLibraryHandler;
- (void)refresh;

@end
