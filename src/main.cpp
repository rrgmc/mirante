#include "Version.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/EnabledModules.h"
#include "items/ItemTypeRegistry.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineParser>

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
                                        QStringLiteral("Load the data folder <folder> instead of the fixtures."),
                                        QStringLiteral("folder"));
    parser.addOption(dataOption);
    parser.process(app);

    // Created in the order of docs/architecture.md#ownership-and-lifetime, and
    // destroyed in reverse. Services that do not exist yet are left out.
    mirante::ItemTypeRegistry types;
    mirante::registerEnabledModules(types);

    mirante::Model model;
    const QString dataDir = parser.isSet(dataOption) ? parser.value(dataOption) : QStringLiteral(":/fixtures");
    const auto problems = mirante::store::load(dataDir, model);

    mirante::MainWindow window(model, types);
    window.setLoadProblems(problems);
    window.show();

    return app.exec();
}
