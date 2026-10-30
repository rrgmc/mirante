#include "platform/Foreground.h"

#include <QApplication>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mirante::platform {

void allowForegroundFor(qint64 pid)
{
#if defined(Q_OS_WIN)
    AllowSetForegroundWindow(pid > 0 ? DWORD(pid) : ASFW_ANY);
#else
    Q_UNUSED(pid);
#endif
}

bool appIsInFront()
{
#if defined(Q_OS_WIN)
    DWORD process = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &process);
    return process == GetCurrentProcessId();
#else
    return QApplication::activeWindow() != nullptr;
#endif
}

} // namespace mirante::platform
