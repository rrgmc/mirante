#include "platform/Launcher.h"

#include "platform/Foreground.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace mirante::launch {

namespace {

const QString kCmd = QStringLiteral("cmd.exe");

// --- Windows ---------------------------------------------------------------

// One argument quoted as the Microsoft C runtime reads it: in double quotes
// when it holds a space, a tab or a quote, with quotes escaped by a backslash
// and the backslashes before them doubled.
QString windowsQuote(const QString &argument)
{
    if (!argument.isEmpty() && !argument.contains(QLatin1Char(' ')) && !argument.contains(QLatin1Char('\t'))
        && !argument.contains(QLatin1Char('"')))
        return argument;
    QString quoted = QStringLiteral("\"");
    int backslashes = 0;
    for (const QChar c : argument) {
        if (c == QLatin1Char('\\')) {
            ++backslashes;
            continue;
        }
        if (c == QLatin1Char('"'))
            quoted += QString(backslashes * 2 + 1, QLatin1Char('\\'));
        else
            quoted += QString(backslashes, QLatin1Char('\\'));
        backslashes = 0;
        quoted += c;
    }
    quoted += QString(backslashes * 2, QLatin1Char('\\'));
    quoted += QLatin1Char('"');
    return quoted;
}

// Text escaped for one pass of cmd.exe: a caret before each character that
// cmd reads as special, quotes included, so cmd sees no quoted region and
// removes exactly one level of carets. Since an escaped quote does not start
// a quoted region, the separators space, ";" and "," are escaped too, or cmd
// would split a path such as "...\Microsoft VS Code\bin\code.cmd" at its
// spaces.
QString cmdEscape(const QString &text)
{
    static const QString special = QStringLiteral("()%!^\"<>&|;, ");
    QString escaped;
    escaped.reserve(text.size() * 2);
    for (const QChar c : text) {
        if (special.contains(c))
            escaped += QLatin1Char('^');
        escaped += c;
    }
    return escaped;
}

// An argument for a program that cmd.exe starts: quoted for the program,
// then escaped for cmd.
QString cmdArgument(const QString &argument)
{
    return cmdEscape(windowsQuote(argument));
}

// wt.exe reads ";" as the start of a new tab, unless escaped as "\;".
QString wtArgument(const QString &argument)
{
    QString escaped = argument;
    escaped.replace(QLatin1Char(';'), QLatin1String("\\;"));
    return escaped;
}

bool isBatchFile(const QString &program)
{
    return program.endsWith(QLatin1String(".cmd"), Qt::CaseInsensitive)
        || program.endsWith(QLatin1String(".bat"), Qt::CaseInsensitive);
}

// The arguments of a cmd.exe command are already quoted for cmd, and are
// passed on the command line exactly as they are. See launch().
[[maybe_unused]] bool isCmd(const Command &command)
{
    return QFileInfo(command.program).fileName().compare(kCmd, Qt::CaseInsensitive) == 0;
}

std::vector<Command> windowsTerminals(const QString &dir, const QString &program, const QStringList &arguments)
{
    Command wt{QStringLiteral("wt.exe"), {QStringLiteral("-d"), wtArgument(dir)}, {}};
    if (!program.isEmpty()) {
        wt.arguments << wtArgument(program);
        for (const QString &argument : arguments)
            wt.arguments << wtArgument(argument);
    }

    // Two passes of cmd: the one that runs start, then the one that start
    // opens. The command is escaped for both.
    Command cmd{kCmd,
                {QStringLiteral("/c"), QStringLiteral("start"), QStringLiteral("\"\""), QStringLiteral("/d"),
                 cmdArgument(dir), QStringLiteral("cmd")},
                {}};
    if (!program.isEmpty()) {
        cmd.arguments << QStringLiteral("/k") << cmdEscape(cmdArgument(program));
        for (const QString &argument : arguments)
            cmd.arguments << cmdEscape(cmdArgument(argument));
    }
    return {wt, cmd};
}

// --- macOS -----------------------------------------------------------------

// One word quoted for a POSIX shell: as it is when it holds only safe
// characters, else in single quotes.
QString shellQuote(const QString &word)
{
    static const QString safe = QStringLiteral("@%+=:,./-_");
    bool plain = !word.isEmpty();
    for (const QChar c : word) {
        if (!(c.isLetterOrNumber() && c.unicode() < 128) && !safe.contains(c)) {
            plain = false;
            break;
        }
    }
    if (plain)
        return word;
    QString quoted = word;
    quoted.replace(QLatin1Char('\''), QLatin1String("'\\''"));
    return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

// Text inside an AppleScript string literal.
QString appleScriptEscape(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    escaped.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return escaped;
}

Command macTerminal(const QString &dir, const QString &program, const QStringList &arguments)
{
    QString line = QStringLiteral("cd ") + shellQuote(dir);
    if (!program.isEmpty()) {
        line += QStringLiteral(" && ") + shellQuote(program);
        for (const QString &argument : arguments)
            line += QLatin1Char(' ') + shellQuote(argument);
    }
    return {QStringLiteral("osascript"),
            {QStringLiteral("-e"), QStringLiteral("tell application \"Terminal\""), QStringLiteral("-e"),
             QStringLiteral("do script \"") + appleScriptEscape(line) + QLatin1Char('"'), QStringLiteral("-e"),
             QStringLiteral("activate"), QStringLiteral("-e"), QStringLiteral("end tell")},
            {}};
}

// --- Linux -----------------------------------------------------------------

std::vector<Command> linuxTerminals(const QString &dir, const QString &program, const QStringList &arguments)
{
    QStringList command;
    if (!program.isEmpty())
        command << program << arguments;

    // x-terminal-emulator and xterm have no working directory flag, so they
    // start in dir.
    Command xte{QStringLiteral("x-terminal-emulator"), {}, dir};
    Command gnome{QStringLiteral("gnome-terminal"), {QStringLiteral("--working-directory=") + dir}, {}};
    Command konsole{QStringLiteral("konsole"), {QStringLiteral("--workdir"), dir}, {}};
    Command xterm{QStringLiteral("xterm"), {}, dir};
    if (!command.isEmpty()) {
        xte.arguments << QStringLiteral("-e") << command;
        gnome.arguments << QStringLiteral("--") << command;
        konsole.arguments << QStringLiteral("-e") << command;
        xterm.arguments << QStringLiteral("-e") << command;
    }
    return {xte, gnome, konsole, xterm};
}

// --- Starting --------------------------------------------------------------

// The program's absolute path: as it is when it holds a path, else found on
// the PATH, where Windows also tries the PATHEXT suffixes, so "code" finds
// "code.cmd". Empty if not found.
QString resolve(const QString &program)
{
    if (program.contains(QLatin1Char('/')) || program.contains(QLatin1Char('\\'))) {
        const QFileInfo info(program);
        return info.isFile() ? info.absoluteFilePath() : QString();
    }
    return QStandardPaths::findExecutable(program);
}

// Starts command detached, with its program at path. Returns whether it
// started.
bool launch(const Command &command, const QString &path)
{
    QProcess process;
    process.setProgram(path);
    process.setWorkingDirectory(command.workingDirectory);
#if defined(Q_OS_WIN)
    if (isCmd(command)) {
        // QProcess would quote the arguments for the C runtime, which cmd does
        // not read; they are already quoted for cmd. No console window of
        // its own: start opens the terminal's window, and a .cmd program such
        // as code needs none.
        process.setNativeArguments(command.arguments.join(QLatin1Char(' ')));
        process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags &= ~DWORD(CREATE_NEW_CONSOLE);
            args->flags |= CREATE_NO_WINDOW;
        });
    } else {
        process.setArguments(command.arguments);
    }
#else
    process.setArguments(command.arguments);
#endif
    qint64 pid = 0;
    if (!process.startDetached(&pid))
        return false;
    // Lets the new window come to the front on Windows.
    mirante::platform::allowForegroundFor(pid);
    return true;
}

} // namespace

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
    const QStringList parts = QProcess::splitCommand(commandTemplate);
    if (!parts.isEmpty()) {
        QStringList expanded;
        for (const QString &part : parts) {
            if (part == QLatin1String("{command}")) {
                if (!program.isEmpty())
                    expanded << program << arguments;
                continue;
            }
            QString argument = part;
            argument.replace(QLatin1String("{dir}"), dir);
            expanded << argument;
        }
        // Started in dir too, for a template without {dir}.
        return {Command{expanded.takeFirst(), expanded, dir}};
    }

    switch (platform) {
    case Platform::Windows:
        return windowsTerminals(dir, program, arguments);
    case Platform::MacOS:
        return {macTerminal(dir, program, arguments)};
    case Platform::Linux:
        return linuxTerminals(dir, program, arguments);
    }
    return {};
}

Command programCommand(Platform platform, const QString &dir, const QString &resolvedProgram,
                       const QStringList &arguments)
{
    if (platform == Platform::Windows && isBatchFile(resolvedProgram)) {
        // Not QDir::toNativeSeparators(), which converts only when the app
        // runs on Windows, so the tests on other platforms would differ.
        QString path = resolvedProgram;
        path.replace(QLatin1Char('/'), QLatin1Char('\\'));
        Command command{kCmd, {QStringLiteral("/c"), cmdArgument(path)}, dir};
        for (const QString &argument : arguments)
            command.arguments << cmdArgument(argument);
        return command;
    }
    return {resolvedProgram, arguments, dir};
}

QString launchTerminal(const QString &commandTemplate, const QString &dir, const QString &program,
                       const QStringList &arguments)
{
    const Platform platform = currentPlatform();
    // On Windows the terminal starts the program with CreateProcess, which
    // does not try .cmd or .bat, so a program such as "claude" installed as
    // claude.cmd is passed by its full path.
    QString target = program;
    if (platform == Platform::Windows && !program.isEmpty()) {
        const QString found = resolve(program);
        if (!found.isEmpty())
            target = QDir::toNativeSeparators(found);
    }

    const std::vector<Command> commands = terminalCommands(platform, commandTemplate, dir, target, arguments);
    for (const Command &command : commands) {
        const QString path = resolve(command.program);
        if (path.isEmpty())
            continue;
        if (!launch(command, path))
            return QStringLiteral("The terminal %1 could not be started.").arg(command.program);
        return {};
    }
    if (!QProcess::splitCommand(commandTemplate).isEmpty() && !commands.empty())
        return QStringLiteral("The terminal %1 was not found.").arg(commands.front().program);
    return QStringLiteral("No terminal was found.");
}

QString startProgram(const QString &dir, const QString &program, const QStringList &arguments)
{
    const QString path = resolve(program);
    if (path.isEmpty())
        return QStringLiteral("%1 was not found.").arg(program);
    const Command command = programCommand(currentPlatform(), dir, path, arguments);
    const QString commandPath = command.program == path ? path : resolve(command.program);
    if (commandPath.isEmpty() || !launch(command, commandPath))
        return QStringLiteral("%1 could not be started.").arg(program);
    return {};
}

void showFolder(const QString &path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

} // namespace mirante::launch
