#include "ui/ItemDialog.h"

#include "items/FieldValidation.h"
#include "items/ItemTypeRegistry.h"
#include "items/UrlMatches.h"
#include "ui/FieldForm.h"

#include <mirante/ItemType.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

namespace mirante {

namespace {

// The type grid: icons of this size, in cells of this size, with up to three
// lines of name under each icon. Names such as "GitLab Merge Request (page)"
// differ only at the end, so they wrap instead of being cut short.
constexpr int kTypeIconSize = 48;
constexpr QSize kTypeCellSize(140, 120);

// Paints an entry of the type grid as Finder's icon view does: the selected
// or hovered entry gets a soft rounded fill around its icon and name, and the
// name keeps the normal text colour.
class TypeGridDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        if (selected || hovered) {
            QColor fill = option.palette.color(QPalette::Highlight);
            fill.setAlphaF(selected ? 0.22f : 0.08f);
            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);
            painter->setPen(Qt::NoPen);
            painter->setBrush(fill);
            painter->drawRoundedRect(QRectF(option.rect).adjusted(4, 2, -4, -2), 8, 8);
            painter->restore();
        }
        QStyleOptionViewItem plain = option;
        plain.state &= ~(QStyle::State_Selected | QStyle::State_MouseOver | QStyle::State_HasFocus);
        QStyledItemDelegate::paint(painter, plain, index);
    }
};

} // namespace

struct ItemDialog::Private
{
    ItemDialog *q = nullptr;
    const ItemTypeRegistry *types = nullptr;
    EditorContext context;
    Item item;
    Item original;                      // as given, for its configuration
    bool creating = false;

    QStackedWidget *pages = nullptr;
    QWidget *typePage = nullptr;        // only when creating
    QWidget *itemPage = nullptr;
    QListWidget *typeList = nullptr;    // only when creating
    QLabel *typeIcon = nullptr;
    QLabel *typeName = nullptr;
    QLineEdit *name = nullptr;
    QFormLayout *form = nullptr;
    QComboBox *webProfile = nullptr;    // shown for types whose views are web views
    QVBoxLayout *editorBox = nullptr;
    ItemEditor *editor = nullptr;       // null for an item of a missing module
    QString editorType;                 // the type the editor was made for
    QLabel *message = nullptr;          // why OK was refused
    QPushButton *back = nullptr;        // only when creating
    QPushButton *next = nullptr;        // only when creating
    QPushButton *ok = nullptr;

    // Making an item from a pasted link. See decision 0049.
    QLineEdit *urlEdit = nullptr;       // only when creating
    QLabel *typeHint = nullptr;         // only when creating
    std::vector<Connection> connections;
    QUrl url;                           // the link in urlEdit, if it is a web address
    std::vector<urlmatches::TypeMatch> matches;     // the types that take url, best first
    QUrl editorUrl;                     // the link the editor was filled from, if any
    QString filledName;                 // the name last filled from a link
    QString urlConnectionId;            // the chosen type's connection from the link

    const urlmatches::TypeMatch *matchFor(const QString &id) const
    {
        for (const urlmatches::TypeMatch &entry : matches) {
            if (entry.type->id() == id)
                return &entry;
        }
        return nullptr;
    }

    // Fills the grid: every type, or the types that take the link, best
    // first. The selected type stays selected if it is still listed.
    void fillTypeList()
    {
        const QListWidgetItem *current = typeList->currentItem();
        const QString selected = current ? current->data(Qt::UserRole).toString() : QString();

        url = urlmatches::webAddress(urlEdit->text());
        matches = urlmatches::match(*types, url, connections);
        std::vector<const ItemType *> shown;
        if (url.isEmpty()) {
            shown = types->types();
        } else {
            for (const urlmatches::TypeMatch &entry : matches)
                shown.push_back(entry.type);
        }

        typeList->clear();
        int row = 0;
        // The registry lists types by name. Names start with their product,
        // such as "GitLab" or "Jira", so the types of a module stay together.
        for (const ItemType *type : shown) {
            auto *entry = new QListWidgetItem(type->icon(), type->name(), typeList);
            entry->setData(Qt::UserRole, type->id());
            entry->setToolTip(type->name());
            entry->setTextAlignment(Qt::AlignHCenter | Qt::AlignTop);
            entry->setSizeHint(kTypeCellSize);
            if (type->id() == selected && !url.isEmpty())
                row = typeList->count() - 1;
        }
        // With a link, the best match is selected first.
        if (url.isEmpty()) {
            for (int i = 0; i < typeList->count(); ++i) {
                if (typeList->item(i)->data(Qt::UserRole).toString() == selected)
                    row = i;
            }
        } else {
            row = 0;
        }
        if (typeList->count() > 0)
            typeList->setCurrentRow(row);
        if (next)
            next->setEnabled(typeList->count() > 0);

        const QString text = urlEdit->text().trimmed();
        if (types->types().empty())
            typeHint->setText(tr("This build has no item types."));
        else if (!text.isEmpty() && url.isEmpty())
            typeHint->setText(tr("This is not a web address, so every type is shown."));
        else if (!url.isEmpty() && matches.empty())
            typeHint->setText(tr("No item type can show this link."));
        else if (!url.isEmpty())
            typeHint->setText(tr("These types can show the link, best first. "
                                 "Double-click one, or select it and choose Next."));
        else
            typeHint->setText(tr("Double-click a type, or select it and choose Next."));
    }

    // Replaces the editor with one for the type of item, showing config.
    void makeEditor(const QJsonObject &config)
    {
        delete editor;
        editor = nullptr;
        editorType = item.type;
        item.config = config;
        if (!types->contains(item.type))
            return;
        const ItemType &itemType = types->type(item.type);
        editor = itemType.createEditor(context, q);
        if (!editor)
            editor = new FieldForm(itemType.fields(), context, q);
        editor->setObjectName(QStringLiteral("editor"));
        editor->setConfig(config);
        editorBox->addWidget(editor);
    }

    // The web profile row is shown only for a type whose views are web views.
    void showWebProfile()
    {
        const bool web = types->contains(item.type) && types->type(item.type).rendering() == Rendering::Web;
        form->setRowVisible(webProfile, web);
    }

    void showTypeName()
    {
        const ItemType &type = types->type(item.type);
        typeName->setText(type.name());
        const QIcon icon = type.icon();
        typeIcon->setPixmap(icon.isNull() ? QPixmap() : icon.pixmap(16, 16));
        typeIcon->setVisible(!icon.isNull());
    }

    // Shows the type page or the item page, with its buttons.
    void showPage(bool typePageShown)
    {
        if (typePage)
            pages->setCurrentWidget(typePageShown ? typePage : itemPage);
        message->hide();
        if (next) {
            next->setVisible(typePageShown);
            back->setVisible(!typePageShown);
        }
        ok->setVisible(!typePageShown);
        // Enter presses the button that goes on.
        (typePageShown ? next : ok)->setDefault(true);
        if (typePageShown)
            typeList->setFocus();
        else
            name->setFocus();
    }

    // Uses the type with id for the new item, and shows the item's page.
    void choose(const QString &id)
    {
        if (!types->contains(id))
            return;
        item.type = id;
        if (const urlmatches::TypeMatch *found = matchFor(id)) {
            // The type's defaults, with what it made of the link over them.
            // The same type from the same link keeps what was entered.
            if (id != editorType || url != editorUrl) {
                QJsonObject config = defaultConfig(types->type(id).fields());
                const QJsonObject &filled = found->match.config;
                for (auto it = filled.begin(); it != filled.end(); ++it)
                    config.insert(it.key(), it.value());
                makeEditor(config);
                editorUrl = url;
            }
            // A name the user typed is kept.
            const QString suggested = found->match.name;
            const QString current = name->text().trimmed();
            if (!suggested.isEmpty() && (current.isEmpty() || current == filledName)) {
                name->setText(suggested);
                filledName = suggested;
            }
            urlConnectionId = found->connectionId;
        } else {
            if (id != editorType || !editorUrl.isEmpty()) {
                // The configuration given with the item belongs to its own type.
                const bool given = id == original.type && !original.config.isEmpty();
                makeEditor(given ? original.config : defaultConfig(types->type(id).fields()));
                editorUrl.clear();
            }
            urlConnectionId.clear();
        }
        showTypeName();
        showWebProfile();
        showPage(false);
    }

    void chooseSelected()
    {
        if (QListWidgetItem *selected = typeList->currentItem())
            choose(selected->data(Qt::UserRole).toString());
    }

    QWidget *makeTypePage()
    {
        auto *page = new QWidget;

        auto *urlLabel = new QLabel(tr("Paste a link to create an item from it:"), page);
        urlEdit = new QLineEdit(page);
        urlEdit->setObjectName(QStringLiteral("url"));
        urlEdit->setPlaceholderText(QStringLiteral("https://"));
        urlEdit->setClearButtonEnabled(true);
        urlLabel->setBuddy(urlEdit);

        auto *heading = new QLabel(tr("Choose the type of the new item."), page);

        typeList = new QListWidget(page);
        typeList->setObjectName(QStringLiteral("types"));
        typeList->setViewMode(QListView::IconMode);
        typeList->setIconSize(QSize(kTypeIconSize, kTypeIconSize));
        typeList->setGridSize(kTypeCellSize);
        typeList->setMovement(QListView::Static);
        typeList->setResizeMode(QListView::Adjust);
        typeList->setWrapping(true);
        typeList->setWordWrap(true);
        // Each entry has the cell's size, so the view gives its text room for
        // three lines. Uniform sizes would take a one-line entry's height.
        typeList->setTextElideMode(Qt::ElideRight);
        typeList->setEditTriggers(QAbstractItemView::NoEditTriggers);
        typeList->setSelectionMode(QAbstractItemView::SingleSelection);
        typeList->setItemDelegate(new TypeGridDelegate(typeList));
        typeList->setMouseTracking(true);

        typeHint = new QLabel(page);
        typeHint->setObjectName(QStringLiteral("typeHint"));
        typeHint->setWordWrap(true);
        typeHint->setForegroundRole(QPalette::PlaceholderText);
        fillTypeList();

        auto *layout = new QVBoxLayout(page);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(urlLabel);
        layout->addWidget(urlEdit);
        layout->addSpacing(6);
        layout->addWidget(heading);
        layout->addWidget(typeList, 1);
        layout->addWidget(typeHint);
        return page;
    }
};

ItemDialog::ItemDialog(const Item &item, bool creating, const ItemTypeRegistry &types, EditorContext context,
                       QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->types = &types;
    d->context = std::move(context);
    d->item = item;
    d->original = item;
    d->creating = creating;

    setWindowTitle(creating ? tr("New item") : tr("Edit item"));
    if (creating)
        resize(660, 480);
    else
        resize(480, 360);

    // The item's page.
    d->itemPage = new QWidget(this);
    auto *form = new QFormLayout;
    d->form = form;

    auto *typeRow = new QHBoxLayout;
    typeRow->setContentsMargins(0, 0, 0, 0);
    d->typeIcon = new QLabel(d->itemPage);
    d->typeName = new QLabel(d->itemPage);
    d->typeName->setObjectName(QStringLiteral("typeName"));
    typeRow->addWidget(d->typeIcon);
    typeRow->addWidget(d->typeName, 1);
    form->addRow(tr("Type:"), typeRow);

    d->name = new QLineEdit(item.name, d->itemPage);
    d->name->setObjectName(QStringLiteral("name"));
    form->addRow(tr("Name:"), d->name);

    d->webProfile = new QComboBox(d->itemPage);
    d->webProfile->setObjectName(QStringLiteral("webProfile"));
    d->webProfile->setToolTip(tr("The item can read the logins and data of every site in its profile."));
    form->addRow(tr("Web profile:"), d->webProfile);

    d->editorBox = new QVBoxLayout;
    d->editorBox->setContentsMargins(0, 0, 0, 0);

    auto *itemLayout = new QVBoxLayout(d->itemPage);
    itemLayout->setContentsMargins(0, 0, 0, 0);
    itemLayout->addLayout(form);
    itemLayout->addLayout(d->editorBox, 1);
    if (!creating && !types.contains(d->item.type)) {
        auto *missing = new QLabel(
            tr("The module of this item type is not available, so only the name can be changed. "
               "The rest of the item is kept."),
            d->itemPage);
        missing->setObjectName(QStringLiteral("missing"));
        missing->setWordWrap(true);
        itemLayout->addWidget(missing);
    }
    itemLayout->addStretch(0);

    d->pages = new QStackedWidget(this);
    if (creating) {
        d->typePage = d->makeTypePage();
        d->pages->addWidget(d->typePage);
    }
    d->pages->addWidget(d->itemPage);

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    d->ok = buttons->button(QDialogButtonBox::Ok);
    if (creating) {
        // Next accepts too: accept() on the type page chooses the type.
        d->next = buttons->addButton(tr("Next"), QDialogButtonBox::AcceptRole);
        d->next->setObjectName(QStringLiteral("next"));
        d->back = buttons->addButton(tr("Back"), QDialogButtonBox::ActionRole);
        d->back->setObjectName(QStringLiteral("back"));
        connect(d->back, &QPushButton::clicked, this, &ItemDialog::back);
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &ItemDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(d->pages, 1);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    setWebProfiles({}, tr("Default"));

    if (creating) {
        // Nothing is edited until a type is chosen.
        d->item.type.clear();
        d->item.config = {};
        form->setRowVisible(d->webProfile, false);
        QListWidget *list = d->typeList;
        int row = 0;
        for (int i = 0; i < list->count(); ++i) {
            if (list->item(i)->data(Qt::UserRole).toString() == item.type)
                row = i;
        }
        if (list->count() > 0)
            list->setCurrentRow(row);
        d->next->setEnabled(list->count() > 0);
        // Enter on the list reaches the dialog, and so presses Next.
        connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *entry) {
            d->typeList->setCurrentItem(entry);
            d->chooseSelected();
        });
        connect(list, &QListWidget::currentRowChanged, this, [this](int current) { d->next->setEnabled(current >= 0); });
        connect(d->urlEdit, &QLineEdit::textChanged, this, [this] { d->fillTypeList(); });
        d->showPage(true);
    } else {
        d->makeEditor(item.config);
        d->showTypeName();
        d->showWebProfile();
        d->showPage(false);
    }
}

ItemDialog::~ItemDialog()
{
    // The type list outlives d, so its signals must not reach it.
    if (d->typeList)
        d->typeList->disconnect(this);
    if (d->urlEdit)
        d->urlEdit->disconnect(this);
}

void ItemDialog::setConnections(const std::vector<Connection> &connections)
{
    d->connections = connections;
    if (d->urlEdit)
        d->fillTypeList();
}

void ItemDialog::setUrl(const QString &text)
{
    if (d->urlEdit)
        d->urlEdit->setText(text.trimmed());
}

QString ItemDialog::urlConnectionId() const { return d->urlConnectionId; }

Item ItemDialog::value() const
{
    Item item = d->item;
    item.name = d->name->text().trimmed();
    if (d->editor)
        item.config = d->editor->config();
    // Only a web item chooses a profile. Another keeps what it had, such as
    // an item of a missing module.
    if (d->form->isRowVisible(d->webProfile))
        item.webProfile = d->webProfile->currentData().toString();
    return item;
}

void ItemDialog::setType(const QString &typeId)
{
    if (!d->creating)
        return;
    for (int i = 0; i < d->typeList->count(); ++i) {
        if (d->typeList->item(i)->data(Qt::UserRole).toString() == typeId)
            d->typeList->setCurrentRow(i);
    }
    d->choose(typeId);
}

bool ItemDialog::choosingType() const { return d->typePage && d->pages->currentWidget() == d->typePage; }

void ItemDialog::back()
{
    if (d->creating)
        d->showPage(true);
}

void ItemDialog::setName(const QString &name) { d->name->setText(name); }

void ItemDialog::setWebProfiles(const std::vector<WebProfile> &profiles, const QString &groupProfileName)
{
    d->webProfile->clear();
    d->webProfile->addItem(tr("The group's profile (%1)").arg(groupProfileName), QString());
    for (const WebProfile &profile : profiles)
        d->webProfile->addItem(profile.name, profile.id);
    // A profile that is not listed, because it was deleted or has not arrived
    // yet, is kept until another is chosen.
    const QString current = d->item.webProfile;
    if (d->webProfile->findData(current) < 0)
        d->webProfile->addItem(tr("Unknown profile, uses the group's"), current);
    d->webProfile->setCurrentIndex(d->webProfile->findData(current));
}

void ItemDialog::setWebProfile(const QString &id)
{
    const int index = d->webProfile->findData(id);
    if (index >= 0)
        d->webProfile->setCurrentIndex(index);
}

bool ItemDialog::showsWebProfile() const { return d->form->isRowVisible(d->webProfile); }

ItemEditor *ItemDialog::editor() const { return d->editor; }

void ItemDialog::accept()
{
    if (choosingType()) {
        d->chooseSelected();
        return;
    }

    QStringList problems;
    if (d->name->text().trimmed().isEmpty())
        problems << tr("Enter a name.");
    if (d->editor)
        problems << d->editor->problems();

    if (!problems.isEmpty()) {
        d->message->setText(problems.join(QLatin1Char('\n')));
        d->message->show();
        return;
    }
    QDialog::accept();
}

} // namespace mirante
