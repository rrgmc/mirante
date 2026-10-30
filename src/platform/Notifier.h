#pragma once

#include <QObject>
#include <QString>

#include <memory>

namespace mirante::platform {

class Tray;

// Desktop notifications. See docs/ui.md#tray-and-notifications.
//
// On Windows and Linux they are the tray icon's messages, in Notifier.cpp. The
// system shows one at a time, so a click always belongs to the last one sent.
// Some Linux desktops do not report the click. On macOS they go through the
// User Notifications framework, in NotifierMac.mm, which asks the user for
// permission the first time.
class Notifier : public QObject
{
    Q_OBJECT

public:
    // tray is used on Windows and Linux, and must outlive the notifier.
    explicit Notifier(Tray *tray, QObject *parent = nullptr);
    ~Notifier() override;

    // Shows a notification. id comes back with clicked().
    void show(const QString &title, const QString &text, const QString &id);

signals:
    void clicked(const QString &id);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante::platform
