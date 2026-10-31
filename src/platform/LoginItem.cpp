#include "platform/LoginItem.h"

// macOS has its own implementation in LoginItemMac.mm.
#if !defined(Q_OS_MACOS)

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

namespace mirante::platform::loginitem {

namespace {

#if defined(Q_OS_WIN)
// The same key and value as the setup's finish page writes, in
// cmake/MiranteInstall.cmake.
const QString runKey = QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");
const QString valueName = QStringLiteral("Mirante");
#else
// The XDG autostart file. See the Desktop Application Autostart Specification.
QString autostartFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/autostart/mirante.desktop");
}
#endif

} // namespace

bool isSupported() { return true; }

#if defined(Q_OS_WIN)

bool isEnabled() { return QSettings(runKey, QSettings::NativeFormat).contains(valueName); }

bool setEnabled(bool enabled, QString *error)
{
    QSettings run(runKey, QSettings::NativeFormat);
    if (enabled) {
        const QString exe = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        run.setValue(valueName, QStringLiteral("\"%1\" --hidden").arg(exe));
    } else {
        run.remove(valueName);
    }
    run.sync();
    if (run.status() != QSettings::NoError) {
        *error = QCoreApplication::translate("LoginItem", "The registry could not be written.");
        return false;
    }
    return true;
}

#else

bool isEnabled() { return QFileInfo::exists(autostartFile()); }

bool setEnabled(bool enabled, QString *error)
{
    const QString path = autostartFile();
    if (!enabled) {
        if (QFile::exists(path) && !QFile::remove(path)) {
            *error = QCoreApplication::translate("LoginItem", "%1 could not be removed.").arg(path);
            return false;
        }
        return true;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        const QString exe = QCoreApplication::applicationFilePath();
        const QString text = QStringLiteral("[Desktop Entry]\n"
                                            "Type=Application\n"
                                            "Name=Mirante\n"
                                            "Exec=\"%1\" --hidden\n"
                                            "Icon=mirante\n"
                                            "Terminal=false\n"
                                            "X-GNOME-Autostart-enabled=true\n")
                                 .arg(exe);
        file.write(text.toUtf8());
        if (file.commit())
            return true;
    }
    *error = QCoreApplication::translate("LoginItem", "%1 could not be written.").arg(path);
    return false;
}

#endif

bool needsApproval() { return false; }

void openSystemSettings() {}

void whenLaunched(std::function<void(bool)> callback) { callback(false); }

} // namespace mirante::platform::loginitem

#endif
