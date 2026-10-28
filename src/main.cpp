#include "Version.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/EnabledModules.h"
#include "items/ItemTypeRegistry.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QSettings>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral(MIRANTE_APP_NAME));
    QApplication::setOrganizationName(QStringLiteral(MIRANTE_ORGANISATION));
    QApplication::setApplicationVersion(QStringLiteral(MIRANTE_VERSION));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption dataOption(QStringLiteral("data"),
                                        QStringLiteral("Open the workspace <folder> instead of the last one."),
                                        QStringLiteral("folder"));
    parser.addOption(dataOption);
    parser.process(app);

    // Created in the order of docs/architecture.md#ownership-and-lifetime, and
    // destroyed in reverse. Services that do not exist yet are left out.
    mirante::ItemTypeRegistry types;
    mirante::registerEnabledModules(types);

    mirante::Model model;
    mirante::MainWindow window(model, types);

    // The workspace given with --data, else the last one used on this device,
    // else the sample data.
    const QString last = QSettings().value(QStringLiteral("workspace/last")).toString();
    if (parser.isSet(dataOption))
        window.openWorkspace(parser.value(dataOption));
    else if (!last.isEmpty() && QDir(last).exists())
        window.openWorkspace(last);
    else
        window.openSampleData();
    window.show();

    return app.exec();
}
