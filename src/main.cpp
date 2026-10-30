#include "Version.h"
#include "api/ApiServices.h"
#include "api/Attention.h"
#include "api/SecretBackend.h"
#include "api/SecretStore.h"
#include "api/ServerClient.h"
#include "api/StatusCache.h"
#include "core/DeviceSettings.h"
#include "core/Language.h"
#include "core/LogBuffer.h"
#include "core/Model.h"
#include "core/Store.h"
#include "items/DashPanel.h"
#include "items/EnabledModules.h"
#include "items/ItemTypeRegistry.h"
#include "items/ViewPool.h"
#include "platform/AttachedDialogs.h"
#include "platform/Foreground.h"
#include "platform/Platform.h"
#include "ui/AppStyle.h"
#include "ui/MainWindow.h"
#include "web/DashSchemeHandler.h"
#include "web/ProfileManager.h"
#include "web/SingleInstance.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QStandardPaths>
#include <QTranslator>

#include <memory>

int main(int argc, char *argv[])
{
    // The log messages for Help > Show Log, kept from the start. Debug builds
    // always keep them; release builds only with --log. The console output
    // stays as it is.
    auto logBuffer = std::make_unique<mirante::LogBuffer>();
    logBuffer->install();

    // Qt WebEngine asks for shared OpenGL contexts, set before the application.
    QCoreApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    // The built-in panels' scheme, which Qt WebEngine needs before the
    // application. See docs/web-views.md#built-in-panels.
    mirante::dash::registerScheme();
    QApplication app(argc, argv);
    QApplication::setStyle(new mirante::AppStyle);
    // On macOS, dialogs stay attached to their window.
    mirante::platform::installAttachedDialogs();
    QApplication::setApplicationName(QStringLiteral(MIRANTE_APP_NAME));
    QApplication::setOrganizationName(QStringLiteral(MIRANTE_ORGANISATION));
    QApplication::setApplicationVersion(QStringLiteral(MIRANTE_VERSION));
    mirante::platform::setAppIcon(QIcon(QStringLiteral(":/icons/mirante.svg")));

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
    const QCommandLineOption webDataOption(
        QStringLiteral("web-data"),
        QStringLiteral("Keep the web logins in <folder> instead, for test runs beside another instance."),
        QStringLiteral("folder"));
    parser.addOption(webDataOption);
    const QCommandLineOption logOption(QStringLiteral("log"),
                                       QStringLiteral("Keep the log messages for Help > Show Log. Always on in debug builds."));
    parser.addOption(logOption);
    const QCommandLineOption noKeychainOption(
        QStringLiteral("no-keychain"),
        QStringLiteral("Keep tokens in memory only, lost at exit, instead of the keychain, for test runs."));
    parser.addOption(noKeychainOption);
    parser.process(app);
#ifdef NDEBUG
    // Without the buffer, Help has no Show Log and Qt's own handler is back.
    if (!parser.isSet(logOption))
        logBuffer.reset();
#endif

    // The device settings come after the application and organisation names,
    // which choose where QSettings keeps them, and before the language, which
    // they hold.
    const auto ownedSettings = parser.isSet(settingsOption)
        ? std::make_unique<mirante::DeviceSettings>(parser.value(settingsOption))
        : std::make_unique<mirante::DeviceSettings>();
    mirante::DeviceSettings &settings = *ownedSettings;

    // The language, before any text is made: Mirante's own translation from
    // the resources, and Qt's, such as the buttons of standard dialogs, from
    // Qt's translations folder. English needs Mirante's file too, for its
    // plural forms. Dates and numbers follow the language. See
    // docs/decisions/0086-english-and-brazilian-portuguese.md.
    const QString language = mirante::language::choose(settings.language(), QLocale::system().uiLanguages());
    QLocale::setDefault(mirante::language::locale(language));
    QTranslator appTranslator;
    if (appTranslator.load(QStringLiteral(":/i18n/mirante_") + language))
        QCoreApplication::installTranslator(&appTranslator);
    else
        qWarning("No translation of Mirante for %s", qPrintable(language));
    QTranslator qtTranslator;
    if (language != QLatin1String("en")) {
        if (qtTranslator.load(QLocale(language), QStringLiteral("qtbase"), QStringLiteral("_"),
                              QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            QCoreApplication::installTranslator(&qtTranslator);
        else
            qWarning("Qt's own translation for %s was not found in %s", qPrintable(language),
                     qPrintable(QLibraryInfo::path(QLibraryInfo::TranslationsPath)));
    }

    // --- One instance per web data folder -------------------------------
    // Two processes must not use the same web logins at once. A later start
    // hands its --data over to the running instance and exits. See
    // docs/decisions/0058-one-instance-per-web-data-folder.md.
    const QString webDataFolder = parser.isSet(webDataOption)
        ? QFileInfo(parser.value(webDataOption)).absoluteFilePath()
        : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/profiles");
    mirante::SingleInstance instance(webDataFolder);
    if (!instance.tryLock()) {
        if (instance.heldElsewhere()) {
            const QString data = parser.isSet(dataOption) ? QFileInfo(parser.value(dataOption)).absoluteFilePath()
                                                          : QString();
            mirante::platform::allowForegroundFor(instance.holderPid());
            if (instance.handOver(data))
                return 0;
        }
        // The holder may have quit meanwhile.
        if (!instance.tryLock()) {
            QMessageBox::critical(
                nullptr, QStringLiteral(MIRANTE_APP_NAME),
                instance.heldElsewhere()
                    ? QCoreApplication::translate("main",
                                                  "Mirante is already running with the web data folder %1, but it "
                                                  "does not answer. Close it, or wait for it, and start Mirante again.")
                          .arg(QDir::toNativeSeparators(webDataFolder))
                    : QCoreApplication::translate("main", "Mirante cannot use the web data folder %1.")
                          .arg(QDir::toNativeSeparators(webDataFolder)));
            return 1;
        }
    }
    // --- End of one instance per web data folder ------------------------

    // Created in the order of docs/architecture.md#ownership-and-lifetime, and
    // destroyed in reverse. Services that do not exist yet are left out. The
    // device settings are made above, before the language.
    mirante::ItemTypeRegistry types;
    // The core's own types first: the built-in panels.
    mirante::dash::registerModule(types);
    mirante::registerEnabledModules(types);

    mirante::Model model;
    // Modules read their settings screens' values from the open workspace's
    // profiles.
    types.setSettingsReader([&model](const QString &profileId, const QString &moduleId, const QString &screenId) {
        return model.moduleSettings(profileId, moduleId, screenId);
    });
    mirante::Store store(model);
    // Tokens, the modules' servers and the status of items. See the M5 plan,
    // part 9. Test runs pass --no-keychain, so they never touch the keychain.
    mirante::SecretStore secrets(parser.isSet(noKeychainOption) ? mirante::makeMemoryBackend()
                                                                 : mirante::makeKeychainBackend());
    mirante::ServerClients servers(model, types, secrets);
    mirante::StatusCache status(model, types, servers, settings);
    mirante::Attention attention(model, status, settings);
    status.setPolling(true);
    // The profiles' web logins outlive the window, and with it every page that uses
    // them. The pool's limit is read once, at start.
    mirante::ProfileManager profiles(webDataFolder);
    // Logins from before user-managed profiles are no longer used. See
    // docs/decisions/0042-user-managed-web-profiles.md.
    profiles.removeOldProfiles();
    mirante::ViewPool pool;
    pool.setLimit(settings.hiddenViewLimit());
    mirante::MainWindow window(model, types, store, settings, &profiles, &pool);
    mirante::ApiServices api;
    api.secrets = &secrets;
    api.servers = &servers;
    api.status = &status;
    api.attention = &attention;
    window.setApiServices(api);

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
    window.startTrayAndHotkey();

    // A later start handed over: open its workspace, if it gave one, and come
    // to the front.
    QObject::connect(&instance, &mirante::SingleInstance::activationRequested, &window,
                     [&window](const QString &dataFolder) {
                         // A dialog may hold entities of the open workspace,
                         // so the workspace is not switched under it: the
                         // window only comes to the front, with the dialog.
                         // The workspace already open is not opened again.
                         QWidget *modal = QApplication::activeModalWidget();
                         const bool other = !dataFolder.isEmpty()
                             && QDir(dataFolder).absolutePath() != QDir(window.workspace()).absolutePath();
                         if (other && !modal)
                             window.openWorkspace(dataFolder);
                         window.showAndRaise();
                     });

    return app.exec();
}
