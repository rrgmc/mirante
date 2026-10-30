#include "platform/GlobalHotkey.h"

#include <QCoreApplication>
#include <QHotkey>

namespace mirante::platform {

struct GlobalHotkey::Private
{
    QHotkey hotkey;
};

GlobalHotkey::GlobalHotkey(QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    connect(&d->hotkey, &QHotkey::activated, this, &GlobalHotkey::activated);
}

GlobalHotkey::~GlobalHotkey() = default;

bool GlobalHotkey::setShortcut(const QKeySequence &sequence, QString *error)
{
    d->hotkey.setRegistered(false);
    if (sequence.isEmpty())
        return true;

    QString reason;
    if (!QHotkey::isPlatformSupported())
        reason = tr("this desktop does not let applications register one. Bind a desktop shortcut to Mirante instead.");
    else if (!d->hotkey.setShortcut(sequence, true))
        reason = tr("another application may already use it.");
    if (reason.isEmpty())
        return true;
    if (error)
        *error = tr("The global hotkey %1 could not be registered: %2")
                     .arg(sequence.toString(QKeySequence::NativeText), reason);
    return false;
}

} // namespace mirante::platform
