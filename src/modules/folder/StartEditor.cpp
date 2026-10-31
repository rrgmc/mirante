#include "StartEditor.h"

#include <mirante/ItemContext.h>

#include <QProcess>
#include <QStringList>

namespace mirante::modules::folder {

QString startEditor(ItemContext &context, const QString &dir, const QString &path)
{
    QStringList parts = QProcess::splitCommand(context.deviceSetting(QStringLiteral("editor")).toString().trimmed());
    if (parts.isEmpty())
        parts << QStringLiteral("code");
    const QString program = parts.takeFirst();
    parts << path;
    return context.startProgram(dir, program, parts);
}

} // namespace mirante::modules::folder
