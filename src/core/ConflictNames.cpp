#include "core/ConflictNames.h"

#include <QRegularExpression>

namespace mirante::conflicts {

namespace {

const QString kWorkspace = QStringLiteral("workspace");
const QString kMirante = QStringLiteral(".mirante");

// A UUID at the start of a name, as QUuid writes it without braces.
const QRegularExpression &uuidPrefix()
{
    static const QRegularExpression re(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}"));
    return re;
}

// Exactly one extension, such as ".json" or ".png".
const QRegularExpression &singleExtension()
{
    static const QRegularExpression re(QStringLiteral("^\\.[0-9A-Za-z]+$"));
    return re;
}

} // namespace

QString originalOf(const QString &fileName)
{
    // Any name that starts with "workspace" and ends with ".mirante" is
    // workspace.mirante or a copy of it.
    if (fileName.startsWith(kWorkspace) && fileName.endsWith(kMirante)) {
        const QString original = kWorkspace + kMirante;
        return fileName == original ? QString() : original;
    }

    // <uuid>.json and <uuid>.<ext> are originals. Any other name that starts
    // with a UUID is a copy.
    const QRegularExpressionMatch match = uuidPrefix().match(fileName);
    if (!match.hasMatch())
        return {};
    const QString uuid = match.captured();
    const QString rest = fileName.mid(uuid.size());
    if (singleExtension().match(rest).hasMatch())
        return {};

    // Sync tools keep the original's extension at the end of the copy's name.
    // Without one, the original is unknown.
    const int dot = rest.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return {};
    const QString extension = rest.mid(dot);
    if (!singleExtension().match(extension).hasMatch())
        return {};
    return uuid + extension;
}

} // namespace mirante::conflicts
