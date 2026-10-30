// The macOS part of Notifier: the User Notifications framework. Built with ARC.
// See Notifier.h.
#include "platform/Notifier.h"

#include <QPointer>

#import <Foundation/Foundation.h>
#import <UserNotifications/UserNotifications.h>

#include <functional>

// Shows notifications while Mirante is in front too, and reports clicks.
@interface MiranteNotificationDelegate : NSObject <UNUserNotificationCenterDelegate>
- (instancetype)initWithCallback:(std::function<void(NSString *)>)callback;
@end

@implementation MiranteNotificationDelegate {
    std::function<void(NSString *)> _callback;
}

- (instancetype)initWithCallback:(std::function<void(NSString *)>)callback
{
    if ((self = [super init]))
        _callback = std::move(callback);
    return self;
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
       willPresentNotification:(UNNotification *)notification
         withCompletionHandler:(void (^)(UNNotificationPresentationOptions))completionHandler
{
    completionHandler(UNNotificationPresentationOptionBanner | UNNotificationPresentationOptionList);
}

- (void)userNotificationCenter:(UNUserNotificationCenter *)center
    didReceiveNotificationResponse:(UNNotificationResponse *)response
             withCompletionHandler:(void (^)(void))completionHandler
{
    if ([response.actionIdentifier isEqualToString:UNNotificationDefaultActionIdentifier]) {
        id value = response.notification.request.content.userInfo[@"id"];
        _callback([value isKindOfClass:[NSString class]] ? (NSString *)value : @"");
    }
    completionHandler();
}

@end

namespace mirante::platform {

struct Notifier::Private
{
    MiranteNotificationDelegate *delegate = nil;
};

Notifier::Notifier(Tray *tray, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    Q_UNUSED(tray);
    QPointer<Notifier> self(this);
    d->delegate = [[MiranteNotificationDelegate alloc] initWithCallback:[self](NSString *id) {
        const QString itemId = QString::fromNSString(id);
        // The delegate is called on the main queue, so the signal is queued
        // only to leave AppKit's handler first.
        QMetaObject::invokeMethod(
            self.data(),
            [self, itemId] {
                if (self)
                    emit self->clicked(itemId);
            },
            Qt::QueuedConnection);
    }];
    UNUserNotificationCenter *center = UNUserNotificationCenter.currentNotificationCenter;
    center.delegate = d->delegate;
    [center requestAuthorizationWithOptions:UNAuthorizationOptionAlert | UNAuthorizationOptionSound
                          completionHandler:^(BOOL, NSError *) {
                          }];
}

Notifier::~Notifier()
{
    UNUserNotificationCenter *center = UNUserNotificationCenter.currentNotificationCenter;
    if (center.delegate == d->delegate)
        center.delegate = nil;
}

void Notifier::show(const QString &title, const QString &text, const QString &id)
{
    UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
    content.title = title.toNSString();
    content.body = text.toNSString();
    content.userInfo = @{@"id" : id.toNSString()};
    UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:NSUUID.UUID.UUIDString
                                                                          content:content
                                                                          trigger:nil];
    // Without permission the system drops the request, and so does Mirante.
    [UNUserNotificationCenter.currentNotificationCenter addNotificationRequest:request
                                                         withCompletionHandler:^(NSError *) {
                                                         }];
}

} // namespace mirante::platform
