#include "ui/ModulesDialog.h"

#include "items/ItemTypeRegistry.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QSplitter>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace mirante {

namespace {

// The scheme of the item type icons in the details pane: icon:<type id>.
const QString kIconScheme = QStringLiteral("icon");

// A read-only pane that draws the icons of item types, which it finds by
// their icon: addresses.
class DetailsBrowser : public QTextBrowser
{
public:
    DetailsBrowser(const ItemTypeRegistry &types, QWidget *parent)
        : QTextBrowser(parent)
        , m_types(types)
    {
    }

    QVariant loadResource(int type, const QUrl &name) override
    {
        if (type == QTextDocument::ImageResource && name.scheme() == kIconScheme) {
            const QString id = name.path();
            if (m_types.contains(id))
                return m_types.type(id).icon().pixmap(QSize(32, 32), devicePixelRatioF());
            return {};
        }
        return QTextBrowser::loadResource(type, name);
    }

private:
    const ItemTypeRegistry &m_types;
};

QString iconTag(const QString &typeId, int size)
{
    return QStringLiteral("<img src=\"%1:%2\" width=\"%3\" height=\"%3\">")
        .arg(kIconScheme, typeId.toHtmlEscaped())
        .arg(size);
}

} // namespace

struct ModulesDialog::Private
{
    const ItemTypeRegistry *types = nullptr;
    QTreeWidget *tree = nullptr;
    QTextBrowser *details = nullptr;

    QString moduleHtml(const QString &moduleId) const;
    QString typeHtml(const QString &typeId) const;
    void showCurrent();
};

QString ModulesDialog::Private::moduleHtml(const QString &moduleId) const
{
    const QString name = types->moduleName(moduleId);
    QString html = QStringLiteral("<h3>%1</h3>").arg(name.toHtmlEscaped());
    const QString description = types->moduleDescription(moduleId);
    if (!description.isEmpty())
        html += QStringLiteral("<p>%1</p>").arg(description.toHtmlEscaped());

    QStringList screens;
    for (const SettingsScreen *screen : types->settingsScreens(moduleId))
        screens << screen->name().toHtmlEscaped();
    if (screens.isEmpty()) {
        html += QStringLiteral("<p>%1</p>").arg(ModulesDialog::tr("No settings."));
    } else {
        html += QStringLiteral("<p><b>%1</b> %2</p><p>%3</p>")
                    .arg(ModulesDialog::tr("Settings:"), screens.join(QStringLiteral(", ")),
                         ModulesDialog::tr("Each profile has its own. Edit them in Profiles &gt; <i>profile</i> &gt; "
                                           "Modules &gt; %1.")
                             .arg(name.toHtmlEscaped()));
    }

    html += QStringLiteral("<h4>%1</h4><table cellspacing=\"4\">").arg(ModulesDialog::tr("Item types"));
    for (const ItemType *type : types->moduleTypes(moduleId)) {
        html += QStringLiteral("<tr><td valign=\"top\">%1</td><td><b>%2</b>")
                    .arg(iconTag(type->id(), 16), type->name().toHtmlEscaped());
        if (!type->description().isEmpty())
            html += QStringLiteral("<br>%1").arg(type->description().toHtmlEscaped());
        html += QStringLiteral("</td></tr>");
    }
    html += QStringLiteral("</table>");
    return html;
}

QString ModulesDialog::Private::typeHtml(const QString &typeId) const
{
    const ItemType &type = types->type(typeId);
    QString html = QStringLiteral("<h3>%1&nbsp;%2</h3>").arg(iconTag(typeId, 24), type.name().toHtmlEscaped());
    if (!type.description().isEmpty())
        html += QStringLiteral("<p>%1</p>").arg(type.description().toHtmlEscaped());
    const QString moduleId = typeId.section(QLatin1Char('.'), 0, 0);
    html += QStringLiteral("<p><b>%1</b> %2</p>")
                .arg(ModulesDialog::tr("Module:"), types->moduleName(moduleId).toHtmlEscaped());
    return html;
}

void ModulesDialog::Private::showCurrent()
{
    const QTreeWidgetItem *item = tree->currentItem();
    if (!item) {
        details->clear();
        return;
    }
    const QString id = item->data(0, Qt::UserRole).toString();
    details->setHtml(item->parent() ? typeHtml(id) : moduleHtml(id));
}

ModulesDialog::ModulesDialog(const ItemTypeRegistry &types, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->types = &types;
    setObjectName(QStringLiteral("modulesDialog"));
    setWindowTitle(tr("Modules"));

    auto *note = new QLabel(tr("The modules built into this copy of Mirante, and the item types each one adds."), this);
    note->setObjectName(QStringLiteral("note"));
    note->setWordWrap(true);

    d->tree = new QTreeWidget(this);
    d->tree->setObjectName(QStringLiteral("modulesTree"));
    d->tree->setHeaderHidden(true);
    d->tree->setRootIsDecorated(true);
    d->tree->setEditTriggers(QAbstractItemView::NoEditTriggers);

    d->details = new DetailsBrowser(types, this);
    d->details->setObjectName(QStringLiteral("details"));
    d->details->setOpenLinks(false);

    // Modules by name, as the user sees them.
    QStringList moduleIds = types.moduleIds();
    std::stable_sort(moduleIds.begin(), moduleIds.end(), [&types](const QString &a, const QString &b) {
        return types.moduleName(a).compare(types.moduleName(b), Qt::CaseInsensitive) < 0;
    });
    for (const QString &moduleId : moduleIds) {
        auto *module = new QTreeWidgetItem(d->tree, QStringList{types.moduleName(moduleId)});
        module->setData(0, Qt::UserRole, moduleId);
        module->setToolTip(0, types.moduleDescription(moduleId));
        QFont font = module->font(0);
        font.setBold(true);
        module->setFont(0, font);
        for (const ItemType *type : types.moduleTypes(moduleId)) {
            auto *row = new QTreeWidgetItem(module, QStringList{type->name()});
            row->setData(0, Qt::UserRole, type->id());
            row->setIcon(0, type->icon());
            row->setToolTip(0, type->description());
        }
    }
    d->tree->expandAll();

    auto *splitter = new QSplitter(this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(d->tree);
    splitter->addWidget(d->details);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({220, 420});

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(note);
    layout->addWidget(splitter, 1);
    layout->addWidget(buttons);

    connect(d->tree, &QTreeWidget::currentItemChanged, this, [this] { d->showCurrent(); });
    if (d->tree->topLevelItemCount() > 0)
        d->tree->setCurrentItem(d->tree->topLevelItem(0));
    else
        d->details->setPlainText(tr("This build has no modules."));

    resize(680, 440);
}

ModulesDialog::~ModulesDialog() = default;

QTreeWidget *ModulesDialog::tree() const { return d->tree; }

QTextBrowser *ModulesDialog::details() const { return d->details; }

} // namespace mirante
