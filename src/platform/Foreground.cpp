#include "platform/Foreground.h"

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

} // namespace mirante::platform
