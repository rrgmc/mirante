#include "platform/Notifier.h"

// macOS has its own implementation in NotifierMac.mm.
#if !defined(Q_OS_MACOS)

#include "platform/Tray.h"

namespace mirante::platform {

struct Notifier::Private
{
    Tray *tray = nullptr;
    QString lastId;
};

Notifier::Notifier(Tray *tray, QObject *parent)
    : QObject(parent)
    , d(std::make_unique<Private>())
{
    d->tray = tray;
    connect(tray, &Tray::messageClicked, this, [this] { emit clicked(d->lastId); });
}

Notifier::~Notifier() = default;

void Notifier::show(const QString &title, const QString &text, const QString &id)
{
    d->lastId = id;
    d->tray->showMessage(title, text);
}

} // namespace mirante::platform

#endif
