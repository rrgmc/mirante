#include "ui/WorkspaceFiles.h"

#include "core/Store.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QLocale>

namespace mirante::workspacefiles {

QString copyIn(Store &store, const QString &path, const std::function<bool(const QString &)> &ask,
               const std::function<void(const QString &)> &tell)
{
    const QFileInfo info(path);
    const QString name = info.fileName();
    if (!info.isFile()) {
        tell(QCoreApplication::translate("WorkspaceFiles", "%1 is not a file.").arg(name));
        return {};
    }
    // A file already in files/ is used as it is, not copied again.
    if (!store.readOnly()) {
        const QFileInfo inFiles(store.filePath(name));
        if (inFiles.exists() && inFiles.canonicalFilePath() == info.canonicalFilePath())
            return name;
    }
    if (store.readOnly()) {
        tell(QCoreApplication::translate("WorkspaceFiles", "This workspace is read-only, so the file cannot be kept in it."));
        return {};
    }
    const qint64 size = info.size();
    if (size > kRefuseAbove) {
        tell(QCoreApplication::translate("WorkspaceFiles", "%1 is too large to keep in the workspace (%2). Put it in a project folder instead.")
                 .arg(name, QLocale().formattedDataSize(size)));
        return {};
    }
    if (size > kAskAbove
        && !ask(QCoreApplication::translate("WorkspaceFiles", "%1 is %2. It will sync to every device of the workspace. Keep it in the workspace?")
                    .arg(name, QLocale().formattedDataSize(size))))
        return {};

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        tell(QCoreApplication::translate("WorkspaceFiles", "%1 could not be read: %2").arg(name, file.errorString()));
        return {};
    }
    const QByteArray bytes = file.readAll();
    QString error;
    const QString saved = store.saveFile(bytes, info.suffix().toLower(), &error);
    if (saved.isEmpty())
        tell(QCoreApplication::translate("WorkspaceFiles", "%1 could not be copied into the workspace: %2").arg(name, error));
    return saved;
}

} // namespace mirante::workspacefiles
