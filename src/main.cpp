#include "Version.h"
#include "core/DeviceSettings.h"
#include "core/LogBuffer.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/EnabledModules.h"
#include "items/ItemTypeRegistry.h"
#include "items/ViewPool.h"
#include "ui/MainWindow.h"
#include "web/ProfileManager.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QStandardPaths>

#include <memory>

int main(int argc, char *argv[])
{
#ifndef NDEBUG
    // Debug builds keep the log messages for Help > Show Log. The console
    // output stays as it is.
    mirante::LogBuffer logBuffer;
    logBuffer.install();
#endif

    // Qt WebEngine asks for shared OpenGL contexts, set before the application.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral(MIRANTE_APP_NAME));
    QApplication::setOrganizationName(QStringLiteral(MIRANTE_ORGANISATION));
    QApplication::setApplicationVersion(QStringLiteral(MIRANTE_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/mirante.svg")));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption dataOption(QStringLiteral("data"),
                                        QStringLiteral("Open the workspace <folder> instead of the last one."),
                                        QStringLiteral("folder"));
    parser.addOption(dataOption);
    const QCommandLineOption settingsOption(
        QStringLiteral("settings"),
        QStringLiteral("Keep this device's settings in the INI <file> instead, for test runs."),
        QStringLiteral("file"));
    parser.addOption(settingsOption);
    parser.process(app);

    // Created in the order of docs/architecture.md#ownership-and-lifetime, and
    // destroyed in reverse. Services that do not exist yet are left out. The
    // settings come after the application and organisation names, which
    // choose where QSettings keeps them.
    const auto ownedSettings = parser.isSet(settingsOption)
        ? std::make_unique<mirante::DeviceSettings>(parser.value(settingsOption))
        : std::make_unique<mirante::DeviceSettings>();
    mirante::DeviceSettings &settings = *ownedSettings;
    mirante::ItemTypeRegistry types;
    mirante::registerEnabledModules(types);

    mirante::Model model;
    mirante::Store store(model);
    // The web profiles outlive the window, and with it every page that uses
    // them. The pool's limit is read once, at start.
    mirante::ProfileManager profiles(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
                                     + QStringLiteral("/profiles"));
    mirante::ViewPool pool;
    pool.setLimit(settings.hiddenViewLimit());
    mirante::MainWindow window(model, types, store, settings, &profiles, &pool);

    // Pending writes reach the disk before the app quits.
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &store, &mirante::Store::flush);

    // The workspace given with --data, else the last one used on this device
    // if it is still a workspace, else the sample data.
    const QString last = settings.lastWorkspace();
    if (parser.isSet(dataOption))
        window.openWorkspace(parser.value(dataOption));
    else if (mirante::store::isWorkspace(last))
        window.openWorkspace(last);
    else
        window.openSampleData();
    window.show();

    return app.exec();
}
