// Linked into every test, so a test started by hand opens no windows on the
// desktop, as under CTest. Setting QT_QPA_PLATFORM first still chooses another
// platform, for watching a test run.
//
// Tests also run in English whatever the system's language: no translator is
// installed, since only main.cpp installs one, and dates and numbers use the
// English locale. See docs/decisions/0086-english-and-brazilian-portuguese.md.
#include <QByteArray>
#include <QLocale>
#include <QtGlobal>

namespace {

const bool offscreenByDefault = [] {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    return true;
}();

} // namespace
