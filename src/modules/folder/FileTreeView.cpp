#include "FileTreeView.h"

#include "StartEditor.h"

#include <mirante/ItemContext.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>

namespace mirante::modules::folder {

namespace {

const QString kFolder = QStringLiteral("folder");
const QString kPath = QStringLiteral("path");
const QString kShowHidden = QStringLiteral("showHidden");

// True for a relative path that stays inside its folder. The configuration
// syncs, so the view checks it.
bool isInsideFolder(QString path)
{
    path.replace(QLatin1Char('\\'), QLatin1Char('/'));
    // The colon also refuses "C:folder" on Windows, which is relative to a drive.
    if (path.startsWith(QLatin1Char('/')) || path.contains(QLatin1Char(':')) || QDir::isAbsolutePath(path))
        return false;
    return !path.split(QLatin1Char('/')).contains(QStringLiteral(".."));
}

// Hides .git below the root, and sorts folders first, on every platform.
class TreeFilter : public QSortFilterProxyModel
{
public:
    using QSortFilterProxyModel::QSortFilterProxyModel;

protected:
    bool filterAcceptsRow(int row, const QModelIndex &parent) const override
    {
        const auto *files = static_cast<const QFileSystemModel *>(sourceModel());
        if (files->fileName(files->index(row, 0, parent)) != QLatin1String(".git"))
            return true;
        // A root inside a folder named .git must still have its parents.
        const QModelIndex root = files->index(files->rootPath());
        for (QModelIndex above = parent; above.isValid(); above = above.parent()) {
            if (above == root)
                return false;
        }
        return true;
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override
    {
        const auto *files = static_cast<const QFileSystemModel *>(sourceModel());
        const bool leftIsDir = files->isDir(left);
        if (leftIsDir != files->isDir(right))
            return leftIsDir;
        return files->fileName(left).compare(files->fileName(right), Qt::CaseInsensitive) < 0;
    }
};

class FileTreeView : public QWidget
{
public:
    FileTreeView(ItemContext &context, QWidget *parent)
        : QWidget(parent)
        , m_context(context)
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);

        m_message = new QLabel(this);
        m_message->setObjectName(QStringLiteral("message"));
        m_message->setTextFormat(Qt::PlainText);
        m_message->setWordWrap(true);
        m_message->setAlignment(Qt::AlignCenter);
        layout->addWidget(m_message);

        m_choose = new QPushButton(QCoreApplication::translate("folder", "Choose folder…"), this);
        m_choose->setObjectName(QStringLiteral("chooseFolder"));
        layout->addWidget(m_choose, 0, Qt::AlignCenter);
        connect(m_choose, &QPushButton::clicked, this, [this] {
            // A modal picker. The core may make the view again after it.
            QPointer<FileTreeView> alive(this);
            m_context.mapFolder(folderId());
            if (alive)
                load();
        });

        m_tree = new QTreeView(this);
        m_tree->setObjectName(QStringLiteral("tree"));
        m_tree->setFrameShape(QFrame::NoFrame);
        m_tree->setHeaderHidden(true);
        m_tree->setUniformRowHeights(true);
        m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
        layout->addWidget(m_tree, 1);
        connect(m_tree, &QTreeView::activated, this, [this](const QModelIndex &index) { activate(index); });

        m_problem = new QLabel(this);
        m_problem->setObjectName(QStringLiteral("problem"));
        m_problem->setTextFormat(Qt::PlainText);
        m_problem->setWordWrap(true);
        m_problem->hide();
        layout->addWidget(m_problem);

        // The menu acts on the selected row, or on the root without one. A
        // right press selects the row under the pointer, or nothing.
        m_tree->setContextMenuPolicy(Qt::ActionsContextMenu);
        addTreeAction(QStringLiteral("openFile"), QCoreApplication::translate("folder", "Open"),
                      [this](const QString &path) { openFile(path); });
        addTreeAction(QStringLiteral("openInEditor"), QCoreApplication::translate("folder", "Open in editor"),
                      [this](const QString &path) { showProblem(startEditor(m_context, m_root, path)); });
        addTreeAction(QStringLiteral("showInFiles"), QCoreApplication::translate("folder", "Show in file manager"),
                      [this](const QString &path) { m_context.showFolder(folderOf(path)); });
        addTreeAction(QStringLiteral("terminalHere"), QCoreApplication::translate("folder", "Open terminal here"),
                      [this](const QString &path) {
            showProblem(m_context.launchTerminal(folderOf(path), QString(), {}));
        });
        addTreeAction(QStringLiteral("copyPath"), QCoreApplication::translate("folder", "Copy path"),
                      [](const QString &path) { QApplication::clipboard()->setText(QDir::toNativeSeparators(path)); });
        addTreeAction(QStringLiteral("copyRelativePath"), QCoreApplication::translate("folder", "Copy relative path"),
                      [this](const QString &path) {
            QApplication::clipboard()->setText(QDir(m_folderRoot).relativeFilePath(path));
        });

        readPath();
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        load();
    }

private:
    QString folderId() const { return m_context.config().value(kFolder).toString(); }

    void addTreeAction(const QString &name, const QString &text, std::function<void(const QString &)> run)
    {
        auto *action = new QAction(text, m_tree);
        action->setObjectName(name);
        connect(action, &QAction::triggered, this, [this, run = std::move(run)] {
            if (m_root.isEmpty())
                return;
            showProblem(QString());
            run(selectedPath());
        });
        m_tree->addAction(action);
    }

    QString selectedPath() const
    {
        const QModelIndexList selected = m_tree->selectionModel() ? m_tree->selectionModel()->selectedRows()
                                                                  : QModelIndexList();
        return selected.isEmpty() || !m_model ? m_root : m_model->filePath(m_filter->mapToSource(selected.first()));
    }

    static QString folderOf(const QString &path)
    {
        const QFileInfo info(path);
        return info.isDir() ? info.absoluteFilePath() : info.absolutePath();
    }

    // Reads where the tree is rooted, and says what is wrong if it cannot show.
    void readPath()
    {
        const QJsonObject config = m_context.config();
        const QString folder = m_context.folderPath(folderId());
        const QString below = config.value(kPath).toString().trimmed();
        m_folderRoot = QDir::cleanPath(folder);
        m_root.clear();
        m_choose->setVisible(folder.isEmpty());

        QString message;
        if (folder.isEmpty()) {
            message = QCoreApplication::translate("folder", "Folder not set on this device");
        } else if (!isInsideFolder(below)) {
            message = QCoreApplication::translate("folder", "The path must be relative and stay inside the folder.");
        } else {
            const QString root = QDir::cleanPath(below.isEmpty() ? folder : QDir(folder).filePath(below));
            if (QFileInfo(root).isDir())
                m_root = root;
            else
                message = QCoreApplication::translate("folder", "The folder is missing: %1")
                              .arg(QDir::toNativeSeparators(root));
        }
        m_message->setText(message);
        m_message->setVisible(!message.isEmpty());
        m_tree->setVisible(!m_root.isEmpty());
    }

    // Reads the path again, and starts reading the files once the view shows,
    // so that a hidden tab costs nothing.
    void load()
    {
        readPath();
        if (m_root.isEmpty() || !isVisible() || m_root == m_modelRoot)
            return;
        delete m_filter;
        delete m_model;

        m_model = new QFileSystemModel(this);
        m_model->setReadOnly(true);
        QDir::Filters filters = QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs;
        if (m_context.config().value(kShowHidden).toBool())
            filters |= QDir::Hidden;
        m_model->setFilter(filters);
        m_filter = new TreeFilter(this);
        m_filter->setSourceModel(m_model);
        m_tree->setModel(m_filter);
        const QModelIndex root = m_model->setRootPath(m_root);
        m_tree->setRootIndex(m_filter->mapFromSource(root));
        for (int column = 1; column < m_model->columnCount(); ++column)
            m_tree->hideColumn(column);
        m_filter->sort(0);
        m_modelRoot = m_root;
    }

    // A double click, or Return on some styles. A folder expands by itself.
    void activate(const QModelIndex &index)
    {
        const QModelIndex source = m_filter->mapToSource(index);
        if (m_model->isDir(source))
            return;
        showProblem(QString());
        openFile(m_model->filePath(source));
    }

    void openFile(const QString &path)
    {
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            showProblem(QCoreApplication::translate("folder", "The file could not be opened."));
    }

    void showProblem(const QString &text)
    {
        m_problem->setText(text);
        m_problem->setVisible(!text.isEmpty());
    }

    ItemContext &m_context;
    QLabel *m_message = nullptr;
    QPushButton *m_choose = nullptr;
    QTreeView *m_tree = nullptr;
    QLabel *m_problem = nullptr;
    QFileSystemModel *m_model = nullptr;
    TreeFilter *m_filter = nullptr;
    QString m_folderRoot;
    QString m_root;
    QString m_modelRoot;
};

} // namespace

QWidget *createFileTreeView(ItemContext &context, QWidget *parent)
{
    return new FileTreeView(context, parent);
}

} // namespace mirante::modules::folder
