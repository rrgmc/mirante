#include "platform/Launcher.h"

namespace mirante::launch {

Platform currentPlatform()
{
#if defined(Q_OS_WIN)
    return Platform::Windows;
#elif defined(Q_OS_MACOS)
    return Platform::MacOS;
#else
    return Platform::Linux;
#endif
}

std::vector<Command> terminalCommands(Platform platform, const QString &commandTemplate, const QString &dir,
                                      const QString &program, const QStringList &arguments)
{
    Q_UNUSED(platform)
    Q_UNUSED(commandTemplate)
    Q_UNUSED(dir)
    Q_UNUSED(program)
    Q_UNUSED(arguments)
    return {};
}

Command programCommand(Platform platform, const QString &dir, const QString &resolvedProgram,
                       const QStringList &arguments)
{
    Q_UNUSED(platform)
    return {resolvedProgram, arguments, dir};
}

QString launchTerminal(const QString &commandTemplate, const QString &dir, const QString &program,
                       const QStringList &arguments)
{
    Q_UNUSED(commandTemplate)
    Q_UNUSED(dir)
    Q_UNUSED(program)
    Q_UNUSED(arguments)
    return QStringLiteral("Not implemented yet.");
}

QString startProgram(const QString &dir, const QString &program, const QStringList &arguments)
{
    Q_UNUSED(dir)
    Q_UNUSED(program)
    Q_UNUSED(arguments)
    return QStringLiteral("Not implemented yet.");
}

void showFolder(const QString &path) { Q_UNUSED(path) }

} // namespace mirante::launch
