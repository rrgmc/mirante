#pragma once

#include <QString>
#include <QStringList>

#include <vector>

// Starts programs, in a terminal or on their own. A program and its arguments
// are passed apart and quoted here for each platform. See
// docs/integrations.md#terminal-launch and
// docs/decisions/0082-commands-as-argument-lists.md. Implemented in Launcher.cpp.

namespace mirante::launch {

enum class Platform { Windows, MacOS, Linux };

// The platform this build runs on. Other Unix systems count as Linux.
Platform currentPlatform();

// One process to start.
struct Command
{
    QString program;
    QStringList arguments;
    QString workingDirectory;   // empty: the app's own

    bool operator==(const Command &other) const
    {
        return program == other.program && arguments == other.arguments
            && workingDirectory == other.workingDirectory;
    }
};

// The commands that would run program with arguments in a new terminal in
// dir, in the order to try them: the first whose program is found is used.
// An empty program opens the terminal alone.
//
// With a template, the device setting "terminal command", the list holds
// one command: the template split into arguments as a shell would, with
// "{dir}" replaced by dir inside any argument, and an argument that is
// exactly "{command}" replaced by program and arguments as separate
// arguments. Without one, the platform's defaults.
std::vector<Command> terminalCommands(Platform platform, const QString &commandTemplate, const QString &dir,
                                      const QString &program, const QStringList &arguments);

// The command that starts a program found at resolvedProgram, an absolute
// path, with arguments in dir. On Windows a .cmd or .bat file runs through
// cmd.exe /c, with its arguments quoted for cmd.
Command programCommand(Platform platform, const QString &dir, const QString &resolvedProgram,
                       const QStringList &arguments);

// Starts program in a new terminal on this platform. Returns empty on
// success, else a message for the user, such as "No terminal was found."
QString launchTerminal(const QString &commandTemplate, const QString &dir, const QString &program,
                       const QStringList &arguments);

// Finds program on the PATH unless it has a path, and starts it detached in
// dir. Returns empty on success, else a message for the user, such as
// "code was not found."
QString startProgram(const QString &dir, const QString &program, const QStringList &arguments);

// Shows a folder in the platform's file manager.
void showFolder(const QString &path);

} // namespace mirante::launch
