#include "platform/Platform.h"

#include <QApplication>

namespace mirante::platform {

QString systemName()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return QStringLiteral("linux");
#endif
}

void setAppIcon(const QIcon &icon)
{
#if defined(Q_OS_MACOS)
    Q_UNUSED(icon);
#else
    QApplication::setWindowIcon(icon);
#endif
}

} // namespace mirante::platform
