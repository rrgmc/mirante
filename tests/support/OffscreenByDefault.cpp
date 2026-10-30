// Linked into every test, so a test started by hand opens no windows on the
// desktop, as under CTest. Setting QT_QPA_PLATFORM first still chooses another
// platform, for watching a test run.
#include <QByteArray>
#include <QtGlobal>

namespace {

const bool offscreenByDefault = [] {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
        qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
    return true;
}();

} // namespace
