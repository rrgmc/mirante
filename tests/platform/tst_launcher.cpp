// Tests Launcher's command building on each platform: the default terminals,
// a terminal alone, a template, quoting of spaces, quotes and ";", and .cmd
// programs on Windows. Nothing is started, except the lookup of a program
// that does not exist.
#include "platform/Launcher.h"

#include <QTest>

using namespace mirante::launch;

namespace {

const QStringList kResume = {QStringLiteral("--resume"), QStringLiteral("0b6f4e4c-2f7e")};

QString describe(const std::vector<Command> &commands)
{
    QStringList lines;
    for (const Command &command : commands)
        lines << command.program + QStringLiteral(" | ") + command.arguments.join(QStringLiteral(" | "))
                     + QStringLiteral(" | in ") + command.workingDirectory;
    return lines.join(QLatin1Char('\n'));
}

} // namespace

class TestLauncher : public QObject
{
    Q_OBJECT

private slots:
    // --- Windows ---

    void windowsDefault()
    {
        const auto commands = terminalCommands(Platform::Windows, {}, QStringLiteral("C:\\work\\my app"),
                                               QStringLiteral("claude"), kResume);
        QCOMPARE(commands.size(), size_t(2));
        QCOMPARE(commands[0], (Command{QStringLiteral("wt.exe"),
                                       {QStringLiteral("-d"), QStringLiteral("C:\\work\\my app"),
                                        QStringLiteral("claude"), QStringLiteral("--resume"),
                                        QStringLiteral("0b6f4e4c-2f7e")},
                                       {}}));
        QCOMPARE(commands[1], (Command{QStringLiteral("cmd.exe"),
                                       {QStringLiteral("/c"), QStringLiteral("start"), QStringLiteral("\"\""),
                                        QStringLiteral("/d"), QStringLiteral("^\"C:\\work\\my^ app^\""),
                                        QStringLiteral("cmd"), QStringLiteral("/k"), QStringLiteral("claude"),
                                        QStringLiteral("--resume"), QStringLiteral("0b6f4e4c-2f7e")},
                                       {}}));
    }

    void windowsTerminalAlone()
    {
        const auto commands =
            terminalCommands(Platform::Windows, {}, QStringLiteral("C:\\work"), QString(), QStringList());
        QCOMPARE(commands.size(), size_t(2));
        QCOMPARE(commands[0], (Command{QStringLiteral("wt.exe"), {QStringLiteral("-d"), QStringLiteral("C:\\work")}, {}}));
        QCOMPARE(commands[1], (Command{QStringLiteral("cmd.exe"),
                                       {QStringLiteral("/c"), QStringLiteral("start"), QStringLiteral("\"\""),
                                        QStringLiteral("/d"), QStringLiteral("C:\\work"), QStringLiteral("cmd")},
                                       {}}));
    }

    // wt.exe reads ";" as a new tab, so it is escaped. QProcess quotes the
    // spaces for wt.exe itself.
    void windowsSemicolon()
    {
        const auto commands = terminalCommands(Platform::Windows, {}, QStringLiteral("C:\\a;b"),
                                               QStringLiteral("tool"), {QStringLiteral("x;y z")});
        QCOMPARE(commands[0].arguments,
                 (QStringList{QStringLiteral("-d"), QStringLiteral("C:\\a\\;b"), QStringLiteral("tool"),
                              QStringLiteral("x\\;y z")}));
    }

    // The cmd fallback passes through two cmd.exe, so the command is escaped
    // twice: a quote becomes ^^^", & becomes ^^^& and a space ^^^ .
    void windowsCmdQuoting()
    {
        const auto commands =
            terminalCommands(Platform::Windows, {}, QStringLiteral("C:\\w"), QStringLiteral("C:\\Program Files\\t.exe"),
                             {QStringLiteral("a&b"), QStringLiteral("say \"hi\""), QStringLiteral("100%")});
        QCOMPARE(commands[1].arguments.mid(6),
                 (QStringList{QStringLiteral("/k"), QStringLiteral("^^^\"C:\\Program^^^ Files\\t.exe^^^\""),
                              QStringLiteral("a^^^&b"), QStringLiteral("^^^\"say^^^ \\^^^\"hi\\^^^\"^^^\""),
                              QStringLiteral("100^^^%")}));
    }

    void windowsProgramCommand()
    {
        QCOMPARE(programCommand(Platform::Windows, QStringLiteral("C:\\w"), QStringLiteral("C:\\bin\\tool.exe"),
                                {QStringLiteral("a b")}),
                 (Command{QStringLiteral("C:\\bin\\tool.exe"), {QStringLiteral("a b")}, QStringLiteral("C:\\w")}));
    }

    // A .cmd or .bat file runs through cmd.exe /c, with its arguments quoted
    // for the program and escaped for cmd.
    void windowsBatchProgram()
    {
        const Command command = programCommand(
            Platform::Windows, QStringLiteral("C:\\w"),
            QStringLiteral("C:/Users/me/AppData/Local/Programs/Microsoft VS Code/bin/code.cmd"),
            {QStringLiteral("C:\\my folder"), QStringLiteral("x&y"), QStringLiteral("plain")});
        QCOMPARE(command,
                 (Command{QStringLiteral("cmd.exe"),
                          {QStringLiteral("/c"),
                           QStringLiteral("^\"C:\\Users\\me\\AppData\\Local\\Programs\\Microsoft^ VS^ Code\\bin\\code.cmd^\""),
                           QStringLiteral("^\"C:\\my^ folder^\""), QStringLiteral("x^&y"), QStringLiteral("plain")},
                          QStringLiteral("C:\\w")}));

        QCOMPARE(programCommand(Platform::Windows, {}, QStringLiteral("C:\\x\\RUN.BAT"), {}).program,
                 QStringLiteral("cmd.exe"));
        // Only on Windows.
        QCOMPARE(programCommand(Platform::Linux, {}, QStringLiteral("/opt/run.cmd"), {}).program,
                 QStringLiteral("/opt/run.cmd"));
    }

    // A trailing backslash inside quotes is doubled, so it does not escape
    // the closing quote.
    void windowsTrailingBackslash()
    {
        const Command command = programCommand(Platform::Windows, {}, QStringLiteral("C:\\x\\code.cmd"),
                                               {QStringLiteral("C:\\my dir\\")});
        QCOMPARE(command.arguments.last(), QStringLiteral("^\"C:\\my^ dir\\\\^\""));
    }

    // --- macOS ---

    void macDefault()
    {
        const auto commands = terminalCommands(Platform::MacOS, {}, QStringLiteral("/Users/me/my app"),
                                               QStringLiteral("claude"), kResume);
        QCOMPARE(commands.size(), size_t(1));
        QCOMPARE(commands[0],
                 (Command{QStringLiteral("osascript"),
                          {QStringLiteral("-e"), QStringLiteral("tell application \"Terminal\""), QStringLiteral("-e"),
                           QStringLiteral("do script \"cd '/Users/me/my app' && claude --resume 0b6f4e4c-2f7e\""),
                           QStringLiteral("-e"), QStringLiteral("activate"), QStringLiteral("-e"),
                           QStringLiteral("end tell")},
                          {}}));
    }

    void macTerminalAlone()
    {
        const auto commands =
            terminalCommands(Platform::MacOS, {}, QStringLiteral("/tmp/w"), QString(), QStringList());
        QCOMPARE(commands[0].arguments.at(3), QStringLiteral("do script \"cd /tmp/w\""));
    }

    // Quoted for the shell first, then escaped for the AppleScript string.
    void macQuoting()
    {
        const auto commands =
            terminalCommands(Platform::MacOS, {}, QStringLiteral("/tmp/it's"), QStringLiteral("echo"),
                             {QStringLiteral("say \"hi\""), QStringLiteral("a\\b"), QStringLiteral("$HOME;rm")});
        QCOMPARE(commands[0].arguments.at(3),
                 QStringLiteral("do script \"cd '/tmp/it'\\\\''s' && echo 'say \\\"hi\\\"' 'a\\\\b' '$HOME;rm'\""));
    }

    // --- Linux ---

    void linuxDefault()
    {
        const auto commands = terminalCommands(Platform::Linux, {}, QStringLiteral("/home/me/my app"),
                                               QStringLiteral("claude"), kResume);
        const QString dir = QStringLiteral("/home/me/my app");
        const std::vector<Command> expected = {
            {QStringLiteral("x-terminal-emulator"),
             {QStringLiteral("-e"), QStringLiteral("claude"), QStringLiteral("--resume"), QStringLiteral("0b6f4e4c-2f7e")},
             dir},
            {QStringLiteral("gnome-terminal"),
             {QStringLiteral("--working-directory=/home/me/my app"), QStringLiteral("--"), QStringLiteral("claude"),
              QStringLiteral("--resume"), QStringLiteral("0b6f4e4c-2f7e")},
             {}},
            {QStringLiteral("konsole"),
             {QStringLiteral("--workdir"), dir, QStringLiteral("-e"), QStringLiteral("claude"),
              QStringLiteral("--resume"), QStringLiteral("0b6f4e4c-2f7e")},
             {}},
            {QStringLiteral("xterm"),
             {QStringLiteral("-e"), QStringLiteral("claude"), QStringLiteral("--resume"), QStringLiteral("0b6f4e4c-2f7e")},
             dir},
        };
        QVERIFY2(commands == expected, qPrintable(describe(commands)));
    }

    void linuxTerminalAlone()
    {
        const auto commands =
            terminalCommands(Platform::Linux, {}, QStringLiteral("/w"), QString(), QStringList());
        const std::vector<Command> expected = {
            {QStringLiteral("x-terminal-emulator"), {}, QStringLiteral("/w")},
            {QStringLiteral("gnome-terminal"), {QStringLiteral("--working-directory=/w")}, {}},
            {QStringLiteral("konsole"), {QStringLiteral("--workdir"), QStringLiteral("/w")}, {}},
            {QStringLiteral("xterm"), {}, QStringLiteral("/w")},
        };
        QVERIFY2(commands == expected, qPrintable(describe(commands)));
    }

    void linuxProgramCommand()
    {
        QCOMPARE(programCommand(Platform::Linux, QStringLiteral("/w"), QStringLiteral("/usr/bin/code"),
                                {QStringLiteral("/w")}),
                 (Command{QStringLiteral("/usr/bin/code"), {QStringLiteral("/w")}, QStringLiteral("/w")}));
    }

    // --- Templates ---

    // Split as a shell would, {dir} replaced inside arguments, and {command}
    // replaced by the program and its arguments as separate arguments. The
    // same on every platform.
    void templateCommand()
    {
        for (const Platform platform : {Platform::Windows, Platform::MacOS, Platform::Linux}) {
            const auto commands = terminalCommands(
                platform, QStringLiteral("alacritty --working-directory {dir} --title \"in {dir}\" -e {command}"),
                QStringLiteral("/home/me/my app"), QStringLiteral("claude"), kResume);
            QCOMPARE(commands.size(), size_t(1));
            QCOMPARE(commands[0],
                     (Command{QStringLiteral("alacritty"),
                              {QStringLiteral("--working-directory"), QStringLiteral("/home/me/my app"),
                               QStringLiteral("--title"), QStringLiteral("in /home/me/my app"), QStringLiteral("-e"),
                               QStringLiteral("claude"), QStringLiteral("--resume"), QStringLiteral("0b6f4e4c-2f7e")},
                              QStringLiteral("/home/me/my app")}));
        }
    }

    // A program with a space in the template stays one argument, and the
    // command's own arguments are never split again.
    void templateQuotedProgram()
    {
        const auto commands = terminalCommands(
            Platform::Windows, QStringLiteral("\"C:\\Program Files\\Term\\term.exe\" -d {dir} {command}"),
            QStringLiteral("C:\\w"), QStringLiteral("tool"), {QStringLiteral("a b;c")});
        QCOMPARE(commands[0],
                 (Command{QStringLiteral("C:\\Program Files\\Term\\term.exe"),
                          {QStringLiteral("-d"), QStringLiteral("C:\\w"), QStringLiteral("tool"), QStringLiteral("a b;c")},
                          QStringLiteral("C:\\w")}));
    }

    // With no program, {command} becomes nothing.
    void templateTerminalAlone()
    {
        const auto commands = terminalCommands(Platform::Linux, QStringLiteral("kitty -d {dir} {command}"),
                                               QStringLiteral("/w"), QString(), QStringList());
        QCOMPARE(commands[0], (Command{QStringLiteral("kitty"), {QStringLiteral("-d"), QStringLiteral("/w")},
                                       QStringLiteral("/w")}));
    }

    // A blank template means the default.
    void blankTemplate()
    {
        const auto commands =
            terminalCommands(Platform::Linux, QStringLiteral("   "), QStringLiteral("/w"), QString(), QStringList());
        QCOMPARE(commands.size(), size_t(4));
    }

    // --- Starting ---

    void missingProgram()
    {
        QCOMPARE(startProgram(QString(), QStringLiteral("mirante-no-such-program"), {}),
                 QStringLiteral("mirante-no-such-program was not found."));
    }

    void missingTemplateTerminal()
    {
        QCOMPARE(launchTerminal(QStringLiteral("mirante-no-such-terminal {command}"), QString(), QString(), {}),
                 QStringLiteral("The terminal mirante-no-such-terminal was not found."));
    }
};

QTEST_MAIN(TestLauncher)
#include "tst_launcher.moc"
