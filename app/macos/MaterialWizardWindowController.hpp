#import <AppKit/AppKit.h>

#include <paperweight/material.hpp>

typedef BOOL (^PWUseWizardMaterialHandler)(
    const paperweight::Material& material,
    NSString* templateIdentifier);
typedef void (^PWWizardSavedMaterialHandler)(NSURL* url);

@interface MaterialWizardWindowController : NSWindowController

- (instancetype)initWithPreviewResolution:(NSUInteger)previewResolution
                        useMaterialHandler:(PWUseWizardMaterialHandler)useMaterialHandler
                      savedMaterialHandler:(PWWizardSavedMaterialHandler)savedMaterialHandler;
- (void)showMaterialWizard;

@end
