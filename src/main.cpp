#include "Version.h"
#include "core/DeviceSettings.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/EnabledModules.h"
#include "items/ItemTypeRegistry.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>

int main(int argc, char *argv[])
{
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
    parser.process(app);

    // Created in the order of docs/architecture.md#ownership-and-lifetime, and
    // destroyed in reverse. Services that do not exist yet are left out. The
    // settings come after the application and organisation names, which
    // choose where QSettings keeps them.
    mirante::DeviceSettings settings;
    mirante::ItemTypeRegistry types;
    mirante::registerEnabledModules(types);

    mirante::Model model;
    mirante::Store store(model);
    mirante::MainWindow window(model, types, store, settings);

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
