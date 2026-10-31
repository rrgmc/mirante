#include "platform/LoginItem.h"

#include <QCoreApplication>

#import <AppKit/AppKit.h>
#import <ServiceManagement/ServiceManagement.h>

namespace mirante::platform::loginitem {

bool isSupported()
{
    if (@available(macOS 13, *))
        return true;
    return false;
}

bool isEnabled()
{
    if (@available(macOS 13, *))
        return SMAppService.mainAppService.status == SMAppServiceStatusEnabled
            || SMAppService.mainAppService.status == SMAppServiceStatusRequiresApproval;
    return false;
}

bool setEnabled(bool enabled, QString *error)
{
    if (@available(macOS 13, *)) {
        SMAppService *service = SMAppService.mainAppService;
        NSError *nsError = nil;
        const BOOL ok = enabled ? [service registerAndReturnError:&nsError] : [service unregisterAndReturnError:&nsError];
        // Registering an item that needs approval reports an error but keeps the item.
        if (ok || (enabled && service.status == SMAppServiceStatusRequiresApproval))
            return true;
        *error = QString::fromNSString(nsError.localizedDescription);
        return false;
    }
    *error = QCoreApplication::translate("LoginItem", "Starting at sign-in needs macOS 13.");
    return false;
}

bool needsApproval()
{
    if (@available(macOS 13, *))
        return SMAppService.mainAppService.status == SMAppServiceStatusRequiresApproval;
    return false;
}

void openSystemSettings()
{
    if (@available(macOS 13, *))
        [SMAppService openSystemSettingsLoginItems];
}

void whenLaunched(std::function<void(bool)> callback)
{
    // The open application event that launched Mirante is current only while
    // applicationDidFinishLaunching runs. At sign-in it says the app is a
    // login item.
    __block id observer = [NSNotificationCenter.defaultCenter
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *) {
                    [NSNotificationCenter.defaultCenter removeObserver:observer];
                    NSAppleEventDescriptor *event = NSAppleEventManager.sharedAppleEventManager.currentAppleEvent;
                    const bool atSignIn = event.eventClass == kCoreEventClass && event.eventID == kAEOpenApplication
                        && [event paramDescriptorForKeyword:keyAEPropData].enumCodeValue == keyAELaunchedAsLogInItem;
                    callback(atSignIn);
                }];
}

} // namespace mirante::platform::loginitem
