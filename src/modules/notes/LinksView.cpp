#include "LinksView.h"

#include "Links.h"

#include <mirante/ItemContext.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QIcon>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLabel>
#include <QList>
#include <QListWidget>
#include <QPainter>
#include <QPointer>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QUrl>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace mirante::modules::notes {

namespace {

constexpr int kDescriptionRole = Qt::UserRole;
constexpr int kMargin = 4;
constexpr int kIconSize = 16;

QFont descriptionFont(QFont font)
{
    if (font.pointSizeF() > 0)
        font.setPointSizeF(font.pointSizeF() * 0.9);
    return font;
}

// A row of a title in the link colour, with its description, smaller, under it.
class LinkDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        QStyleOptionViewItem item(option);
        initStyleOption(&item, index);
        const QString title = item.text;
        const QIcon icon = item.icon;
        item.text.clear();
        item.icon = QIcon();
        const QStyle *style = item.widget ? item.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);

        const bool selected = item.state & QStyle::State_Selected;
        const QRect area = item.rect.adjusted(kMargin, kMargin, -kMargin, -kMargin);
        const QFontMetrics titleMetrics(item.font);
        const QRect iconRect(area.left(), area.top() + (titleMetrics.height() - kIconSize) / 2, kIconSize, kIconSize);
        icon.paint(painter, iconRect);

        const QRect text = area.adjusted(kIconSize + kMargin, 0, 0, 0);
        painter->save();
        painter->setFont(item.font);
        painter->setPen(item.palette.color(selected ? QPalette::HighlightedText : QPalette::Link));
        painter->drawText(text, Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
                          titleMetrics.elidedText(title, Qt::ElideRight, text.width()));
        const QString description = index.data(kDescriptionRole).toString();
        if (!description.isEmpty()) {
            const QFont small = descriptionFont(item.font);
            const QFontMetrics smallMetrics(small);
            painter->setFont(small);
            painter->setPen(item.palette.color(selected ? QPalette::HighlightedText : QPalette::PlaceholderText));
            painter->drawText(text.adjusted(0, titleMetrics.height(), 0, 0),
                              Qt::AlignLeft | Qt::AlignTop | Qt::TextSingleLine,
                              smallMetrics.elidedText(description, Qt::ElideRight, text.width()));
        }
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        int height = QFontMetrics(option.font).height();
        if (!index.data(kDescriptionRole).toString().isEmpty())
            height += QFontMetrics(descriptionFont(option.font)).height();
        return {0, height + 2 * kMargin};
    }
};

// A list where Return also opens the current row.
class LinkList : public QListWidget
{
public:
    using QListWidget::QListWidget;
    std::function<void()> onReturn;

protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && onReturn) {
            onReturn();
            return;
        }
        QListWidget::keyPressEvent(event);
    }
};

class LinksView : public QWidget
{
public:
    LinksView(ItemContext &context, QWidget *parent)
        : QWidget(parent)
        , m_context(context)
        , m_links(linksOf(context.config()))
    {
        auto *layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        auto *list = new LinkList(this);
        m_list = list;
        m_list->setObjectName(QStringLiteral("links"));
        m_list->setFrameShape(QFrame::NoFrame);
        m_list->setItemDelegate(new LinkDelegate(m_list));
        m_list->setMouseTracking(true);
        m_list->setCursor(Qt::PointingHandCursor);
        layout->addWidget(m_list, 1);
        m_problem = new QLabel(this);
        m_problem->setObjectName(QStringLiteral("problem"));
        m_problem->setTextFormat(Qt::PlainText);
        m_problem->setWordWrap(true);
        m_problem->hide();
        layout->addWidget(m_problem);

        const QIcon web(QStringLiteral(":/modules/notes/icons/links.svg"));
        for (const QJsonObject &link : std::as_const(m_links)) {
            auto *row = new QListWidgetItem(linkTitle(link), m_list);
            row->setData(kDescriptionRole, link.value(kLinkDescription).toString());
            row->setToolTip(addressOf(link));
            switch (linkKind(link)) {
            case LinkKind::Url:
                row->setIcon(web);
                break;
            case LinkKind::Project:
                row->setIcon(style()->standardIcon(link.value(kLinkPath).toString().isEmpty() ? QStyle::SP_DirIcon
                                                                                              : QStyle::SP_FileIcon));
                break;
            case LinkKind::Workspace:
                row->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
                break;
            }
        }

        connect(m_list, &QListWidget::itemClicked, this, [this](QListWidgetItem *row) { open(m_list->row(row)); });
        list->onReturn = [this] { open(m_list->currentRow()); };

        // The menu acts on the row under the pointer, which a right press
        // makes the current one.
        m_list->setContextMenuPolicy(Qt::ActionsContextMenu);
        addRowAction(QStringLiteral("openLink"), QCoreApplication::translate("notes", "Open"),
                     [this](int row) { open(row); });
        addRowAction(QStringLiteral("copyLink"), QCoreApplication::translate("notes", "Copy address"), [this](int row) {
            QApplication::clipboard()->setText(addressOf(m_links.at(row)));
        });
        m_showInFiles = addRowAction(QStringLiteral("showLink"),
                                     QCoreApplication::translate("notes", "Show in file manager"),
                                     [this](int row) { showInFiles(row); });
        connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
            m_showInFiles->setEnabled(row >= 0 && linkKind(m_links.at(row)) != LinkKind::Url);
        });
        m_showInFiles->setEnabled(false);
    }

private:
    QAction *addRowAction(const QString &name, const QString &text, std::function<void(int)> run)
    {
        auto *action = new QAction(text, m_list);
        action->setObjectName(name);
        connect(action, &QAction::triggered, this, [this, run = std::move(run)] {
            const int row = m_list->currentRow();
            if (row >= 0)
                run(row);
        });
        m_list->addAction(action);
        return action;
    }

    // The link's file on this device, or empty with no message: the folder has
    // no local path, or the link is not valid.
    QString localPath(const QJsonObject &link) const
    {
        if (linkKind(link) == LinkKind::Workspace)
            return m_context.filePath(link.value(kLinkFile).toString());
        const QString path = link.value(kLinkPath).toString();
        const QString folder = m_context.folderPath(link.value(kLinkFolder).toString());
        if (folder.isEmpty() || !isInsideFolder(path))
            return {};
        return path.isEmpty() ? folder : QDir(folder).filePath(path);
    }

    // What the tooltip shows and Copy address copies: the address or the local
    // path, or the path in the folder where it has no local path yet.
    QString addressOf(const QJsonObject &link) const
    {
        if (linkKind(link) == LinkKind::Url)
            return link.value(kLinkUrl).toString();
        const QString local = localPath(link);
        return local.isEmpty() ? linkTitle(link) : QDir::toNativeSeparators(local);
    }

    void open(int row)
    {
        if (row < 0 || row >= m_links.size())
            return;
        showProblem(QString());
        // A copy, since the core may make the view again after mapFolder().
        const QJsonObject link = m_links.at(row);
        if (const QString problem = linkProblem(link); !problem.isEmpty()) {
            showProblem(problem);
            return;
        }
        if (linkKind(link) == LinkKind::Url) {
            openUrl(QUrl(link.value(kLinkUrl).toString().trimmed()));
            return;
        }
        if (linkKind(link) == LinkKind::Workspace) {
            const QString path = localPath(link);
            if (path.isEmpty())
                showProblem(QCoreApplication::translate("notes",
                                                        "The file is missing. It may not have synced to this device yet."));
            else
                openUrl(QUrl::fromLocalFile(path));
            return;
        }

        const QString folderId = link.value(kLinkFolder).toString();
        if (m_context.folderPath(folderId).isEmpty()) {
            QPointer<LinksView> alive(this);
            m_context.mapFolder(folderId);
            if (!alive || m_context.folderPath(folderId).isEmpty())
                return;
        }
        const QString path = localPath(link);
        if (!QFileInfo::exists(path))
            showProblem(QCoreApplication::translate("notes", "The file is missing: %1").arg(linkTitle(link)));
        else
            openUrl(QUrl::fromLocalFile(path));
    }

    void openUrl(const QUrl &url)
    {
        if (!QDesktopServices::openUrl(url))
            showProblem(QCoreApplication::translate("notes", "The link could not be opened."));
    }

    void showInFiles(int row)
    {
        const QString path = localPath(m_links.at(row));
        if (path.isEmpty())
            return;
        const QFileInfo info(path);
        m_context.showFolder(info.isDir() ? info.absoluteFilePath() : info.absolutePath());
    }

    void showProblem(const QString &text)
    {
        m_problem->setText(text);
        m_problem->setVisible(!text.isEmpty());
    }

    ItemContext &m_context;
    QList<QJsonObject> m_links;
    QListWidget *m_list = nullptr;
    QLabel *m_problem = nullptr;
    QAction *m_showInFiles = nullptr;
};

} // namespace

QWidget *createLinksView(ItemContext &context, QWidget *parent)
{
    return new LinksView(context, parent);
}

} // namespace mirante::modules::notes
