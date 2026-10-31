// The document module: a Markdown, plain text or PDF file from a web
// address, the workspace's files/ or a project folder. See README.md and
// docs/decisions/0104-document-items.md.
#include "DocumentEditor.h"
#include "DocumentFormat.h"
#include "DocumentLinks.h"
#include "DocumentSource.h"
#include "DocumentView.h"

#include <mirante/Fields.h>
#include <mirante/ItemContext.h>
#include <mirante/ItemType.h>
#include <mirante/Module.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <QUuid>

#include <memory>
#include <vector>

namespace mirante::modules::document {

namespace {

// How often a missing workspace file is looked for again, since the core
// makes no event for a file that sync delivers to files/.
constexpr int kRetryMissingMs = 10 * 1000;

class DocumentType : public ItemType
{
public:
    DocumentType(const QString &id, const QString &name, const QString &description, const QString &icon)
        : m_id(id)
        , m_name(name)
        , m_description(description)
        , m_icon(QStringLiteral(":/modules/document/icons/") + icon)
    {
    }

    QString id() const override { return m_id; }
    QString name() const override { return m_name; }
    QIcon icon() const override { return m_icon; }
    QString shortName() const override { return QCoreApplication::translate("document", "Doc"); }
    QString description() const override { return m_description; }
    Rendering rendering() const override { return Rendering::Native; }

private:
    QString m_id;
    QString m_name;
    QString m_description;
    QIcon m_icon;
};

// Document (web address).
class UrlDocumentType : public DocumentType
{
public:
    UrlDocumentType()
        : DocumentType(QStringLiteral("document.url"),
                       QCoreApplication::translate("document", "Document (web address)"),
                       QCoreApplication::translate("document",
                                                   "A Markdown, text or PDF file at a public web address, such as "
                                                   "a README on GitHub, downloaded again on Reload or on a timer."),
                       QStringLiteral("document-url.svg"))
    {
    }

    std::vector<Field> fields() const override
    {
        Field url;
        url.key = QStringLiteral("url");
        url.label = QCoreApplication::translate("document", "Address");
        url.type = Field::Type::Url;
        url.required = true;
        url.placeholder = QStringLiteral("https://");

        Field refresh;
        refresh.key = QStringLiteral("refresh");
        refresh.label = QCoreApplication::translate("document", "Refresh (minutes, 0 for never)");
        refresh.type = Field::Type::Integer;
        refresh.defaultValue = 0;
        return {url, formatField(), refresh};
    }

    std::optional<UrlMatch> fromUrl(const QUrl &url, const QString &profileId) const override
    {
        Q_UNUSED(profileId)
        return urlMatch(url);
    }

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        const QJsonObject config = context.config();
        const QUrl url(config.value(QStringLiteral("url")).toString());
        if (!url.isValid() || (url.scheme() != QLatin1String("http") && url.scheme() != QLatin1String("https")))
            return context.createMessageView(
                QCoreApplication::translate("document", "Set the address in the item's settings."), parent);
        const int refresh = config.value(QStringLiteral("refresh")).toInt();
        return createDocumentView(context, std::make_unique<UrlSource>(url, refresh), false, parent);
    }
};

// Document (workspace file).
class WorkspaceDocumentType : public DocumentType
{
public:
    WorkspaceDocumentType()
        : DocumentType(QStringLiteral("document.file"),
                       QCoreApplication::translate("document", "Document (workspace file)"),
                       QCoreApplication::translate("document",
                                                   "A copy of a Markdown, text or PDF file, kept in the workspace "
                                                   "so it syncs to your other devices. Choosing or dropping a file "
                                                   "copies it."),
                       QStringLiteral("document-file.svg"))
    {
    }

    // For the defaults of a dropped file. The editor edits the configuration.
    std::vector<Field> fields() const override { return {formatField()}; }

    ItemEditor *createEditor(const EditorContext &context, QWidget *parent) const override
    {
        return new DocumentEditor(context, parent);
    }

    std::optional<FileMatch> fromFile(const FileDrop &drop) const override { return workspaceFileMatch(drop); }

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        const QJsonObject config = context.config();
        const QString file = config.value(QStringLiteral("file")).toString();
        if (file.isEmpty())
            return context.createMessageView(
                QCoreApplication::translate("document", "Choose the file in the item's settings."), parent);
        FileSource::Options options;
        options.path = [&context, file] { return context.filePath(file); };
        options.fileName = config.value(QStringLiteral("originalName")).toString();
        options.missingMessage = QCoreApplication::translate(
            "document", "The file is missing. It may not have synced to this device yet.");
        options.retryMissingMs = kRetryMissingMs;
        return createDocumentView(context, std::make_unique<FileSource>(std::move(options)), false, parent);
    }
};

// Document (project file).
class ProjectDocumentType : public DocumentType
{
public:
    ProjectDocumentType()
        : DocumentType(QStringLiteral("document.project"),
                       QCoreApplication::translate("document", "Document (project file)"),
                       QCoreApplication::translate("document",
                                                   "A Markdown, text or PDF file in one of the project's folders, "
                                                   "shown again when it changes on disk."),
                       QStringLiteral("document-project.svg"))
    {
    }

    std::vector<Field> fields() const override
    {
        Field folder;
        folder.key = QStringLiteral("folder");
        folder.label = QCoreApplication::translate("document", "Folder");
        folder.type = Field::Type::Folder;
        folder.required = true;

        Field path;
        path.key = QStringLiteral("path");
        path.label = QCoreApplication::translate("document", "Path in the folder");
        path.required = true;
        path.placeholder = QStringLiteral("docs/README.md");
        return {folder, path, formatField()};
    }

    std::optional<FileMatch> fromFile(const FileDrop &drop) const override { return projectFileMatch(drop); }

    QWidget *createView(ItemContext &context, QWidget *parent) const override
    {
        const QJsonObject config = context.config();
        const QString folderId = config.value(QStringLiteral("folder")).toString();
        const QString path = QDir::fromNativeSeparators(config.value(QStringLiteral("path")).toString().trimmed());
        if (path.isEmpty())
            return context.createMessageView(
                QCoreApplication::translate("document", "Set the path in the item's settings."), parent);
        if (QDir::isAbsolutePath(path))
            return context.createMessageView(
                QCoreApplication::translate("document", "The path must be relative to the folder."), parent);
        const QString root = context.folderPath(folderId);
        if (root.isEmpty())
            return createChooseFolderView(context, folderId, parent);

        FileSource::Options options;
        options.path = [root, path] { return QDir(root).filePath(path); };
        options.missingMessage = QCoreApplication::translate("document", "The file is missing.");
        options.localRoot = root;
        return createDocumentView(context, std::make_unique<FileSource>(std::move(options)), true, parent);
    }
};

} // namespace

void registerModule(ModuleRegistrar &registrar)
{
    registrar.setModuleName(QCoreApplication::translate("document", "Documents"));
    registrar.setModuleDescription(QCoreApplication::translate(
        "document", "Markdown, text and PDF files, from a web address, the workspace or a project folder."));
    registrar.setModuleUuid(QUuid(QStringLiteral("21798809-decf-4f90-9029-795457d83198")));
    registrar.setModuleVersion(QStringLiteral("0.1.0"));
    registrar.setModuleAuthor(QStringLiteral("Rangel Reale"));
    registrar.setModuleHomepage(
        QUrl(QStringLiteral("https://github.com/rrgmc/mirante/tree/master/src/modules/document")));
    registrar.addItemType(std::make_unique<UrlDocumentType>());
    registrar.addItemType(std::make_unique<WorkspaceDocumentType>());
    registrar.addItemType(std::make_unique<ProjectDocumentType>());
}

} // namespace mirante::modules::document
