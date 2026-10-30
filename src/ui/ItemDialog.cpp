#include "ui/ItemDialog.h"

#include "items/FieldValidation.h"
#include "items/ItemTypeRegistry.h"
#include "items/UrlMatches.h"
#include "ui/FieldForm.h"

#include <mirante/ItemType.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSignalBlocker>
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
    QComboBox *profile = nullptr;       // shown for types that use a profile
    QCheckBox *watch = nullptr;         // shown for types that report status
    QWidget *pageKeys = nullptr;        // shown for types whose views are web views
    QListWidget *pageKeyList = nullptr; // the keys, in PortableText as data
    QString groupProfile;               // the profile of the item's group
    QStringList profileIds;             // the profiles offered, without an unknown one
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
    QUrl url;                           // the link in urlEdit, if it is a web address
    std::vector<urlmatches::TypeMatch> matches;     // the types that take url, best first
    QUrl editorUrl;                     // the link the editor was filled from, if any
    QString filledName;                 // the name last filled from a link

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
        // A link is read in the profile a new item uses: its group's.
        matches = urlmatches::match(*types, url, groupProfile);
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
            editor = new FieldForm(itemType.fieldsForProfile(effectiveProfile()), context, q);
        editor->setObjectName(QStringLiteral("editor"));
        editor->setConfig(config);
        editorBox->addWidget(editor);
    }

    // The profile row is shown only for a type that uses a profile, and the
    // watch row only for a type that reports status, since only those have
    // attention. See decision 0087.
    void showTypeRows()
    {
        form->setRowVisible(profile, types->usesProfile(item.type));
        form->setRowVisible(watch, types->contains(item.type) && types->type(item.type).hasStatus());
        // A built-in panel takes no keys of its own.
        form->setRowVisible(pageKeys, types->contains(item.type)
                                          && types->type(item.type).rendering() == Rendering::Web
                                          && !types->isBuiltIn(item.type.section(QLatin1Char('.'), 0, 0)));
    }

    // The keys the item's web page keeps instead of the app's shortcuts:
    // a list, a field that records a key, Add and Remove. See decision 0088.
    void buildPageKeys(const QStringList &keys)
    {
        pageKeys = new QWidget(itemPage);
        pageKeys->setObjectName(QStringLiteral("pageKeys"));
        pageKeys->setToolTip(ItemDialog::tr("While the page has the keyboard focus, these keys go to the page "
                                            "instead of running Mirante's shortcuts."));
        pageKeyList = new QListWidget(pageKeys);
        pageKeyList->setObjectName(QStringLiteral("pageKeyList"));
        pageKeyList->setMaximumHeight(pageKeyList->fontMetrics().height() * 5);
        for (const QString &key : keys)
            addPageKey(QKeySequence::fromString(key, QKeySequence::PortableText));

        auto *record = new QKeySequenceEdit(pageKeys);
        record->setObjectName(QStringLiteral("pageKeyEdit"));
        record->setMaximumSequenceLength(1);
        record->setClearButtonEnabled(true);
        auto *add = new QPushButton(ItemDialog::tr("Add"), pageKeys);
        add->setObjectName(QStringLiteral("addPageKey"));
        auto *remove = new QPushButton(ItemDialog::tr("Remove"), pageKeys);
        remove->setObjectName(QStringLiteral("removePageKey"));
        const auto enable = [this, record, add, remove] {
            add->setEnabled(!record->keySequence().isEmpty());
            remove->setEnabled(pageKeyList->currentRow() >= 0);
        };
        QObject::connect(record, &QKeySequenceEdit::keySequenceChanged, q, enable);
        QObject::connect(pageKeyList, &QListWidget::currentRowChanged, q, enable);
        QObject::connect(add, &QPushButton::clicked, q, [this, record] {
            addPageKey(record->keySequence());
            record->clear();
        });
        QObject::connect(remove, &QPushButton::clicked, q, [this] { delete pageKeyList->currentItem(); });
        enable();

        auto *buttons = new QHBoxLayout;
        buttons->setContentsMargins(0, 0, 0, 0);
        buttons->addWidget(record, 1);
        buttons->addWidget(add);
        buttons->addWidget(remove);
        auto *layout = new QVBoxLayout(pageKeys);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(pageKeyList);
        layout->addLayout(buttons);
    }

    // Adds a key once, shown in the platform's words.
    void addPageKey(const QKeySequence &key)
    {
        const QString portable = key.toString(QKeySequence::PortableText);
        if (key.isEmpty() || !pageKeyList->findItems(key.toString(QKeySequence::NativeText), Qt::MatchExactly).isEmpty())
            return;
        auto *entry = new QListWidgetItem(key.toString(QKeySequence::NativeText), pageKeyList);
        entry->setData(Qt::UserRole, portable);
    }

    QStringList pageKeyValues() const
    {
        QStringList keys;
        for (int i = 0; i < pageKeyList->count(); ++i)
            keys << pageKeyList->item(i)->data(Qt::UserRole).toString();
        return keys;
    }

    // The profile the item would use with the chosen entry: its own, or else
    // its group's. An unknown profile falls back to the group's.
    QString effectiveProfile() const
    {
        const QString chosen = profile->currentData().toString();
        return profileIds.contains(chosen) ? chosen : groupProfile;
    }

    // The fields of a form may depend on the profile, such as the
    // suggestions of a module's settings, so the form is built again for
    // another profile, keeping what was entered. A type's own editor stays.
    void profileChosen()
    {
        if (editor && qobject_cast<FieldForm *>(editor) && types->contains(item.type))
            makeEditor(editor->config());
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
        } else {
            if (id != editorType || !editorUrl.isEmpty()) {
                // The configuration given with the item belongs to its own type.
                const bool given = id == original.type && !original.config.isEmpty();
                makeEditor(given ? original.config : defaultConfig(types->type(id).fields()));
                editorUrl.clear();
            }
        }
        showTypeName();
        showTypeRows();
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

    d->profile = new QComboBox(d->itemPage);
    d->profile->setObjectName(QStringLiteral("profile"));
    d->profile->setToolTip(tr("The item uses the logins and module settings of its profile. It can "
                              "read the logins and data of every site in the profile."));
    form->addRow(tr("Profile:"), d->profile);

    d->watch = new QCheckBox(tr("Count on the tray icon, and notify"), d->itemPage);
    d->watch->setObjectName(QStringLiteral("watch"));
    d->watch->setChecked(item.watch);
    d->watch->setToolTip(tr("A watched item that needs attention counts on the tray icon, and a failed "
                            "pipeline sends a desktop notification. The sidebar shows every item's "
                            "attention either way."));
    form->addRow(tr("Watch:"), d->watch);

    d->buildPageKeys(item.pageKeys);
    form->addRow(tr("Keys for the page:"), d->pageKeys);

    d->editorBox = new QVBoxLayout;
    d->editorBox->setContentsMargins(0, 0, 0, 0);

    auto *itemLayout = new QVBoxLayout(d->itemPage);
    itemLayout->setContentsMargins(0, 0, 0, 0);
    itemLayout->addLayout(form);
    itemLayout->addLayout(d->editorBox, 1);
    if (!creating && !types.contains(d->item.type)) {
        // A type whose module is in the build was removed from it; see
        // decision 0084.
        const bool removed = types.moduleIds().contains(d->item.type.section(QLatin1Char('.'), 0, 0));
        auto *missing = new QLabel(
            removed ? tr("This item type is no longer available, so only the name can be changed. "
                         "The rest of the item is kept.")
                    : tr("The module of this item type is not available, so only the name can be changed. "
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

    setProfiles({}, kDefaultProfile, tr("Default"));

    if (creating) {
        // Nothing is edited until a type is chosen.
        d->item.type.clear();
        d->item.config = {};
        form->setRowVisible(d->profile, false);
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
        d->showTypeRows();
        d->showPage(false);
    }
    connect(d->profile, &QComboBox::currentIndexChanged, this, [this] { d->profileChosen(); });
}

ItemDialog::~ItemDialog()
{
    // The type list outlives d, so its signals must not reach it.
    if (d->typeList)
        d->typeList->disconnect(this);
    if (d->urlEdit)
        d->urlEdit->disconnect(this);
}

void ItemDialog::setUrl(const QString &text)
{
    if (d->urlEdit)
        d->urlEdit->setText(text.trimmed());
}

Item ItemDialog::value() const
{
    Item item = d->item;
    item.name = d->name->text().trimmed();
    if (d->editor)
        item.config = d->editor->config();
    // Only an item whose type uses a profile chooses one. Another keeps what
    // it had, such as an item of a missing module.
    if (d->form->isRowVisible(d->profile))
        item.profile = d->profile->currentData().toString();
    if (d->form->isRowVisible(d->watch))
        item.watch = d->watch->isChecked();
    if (d->form->isRowVisible(d->pageKeys))
        item.pageKeys = d->pageKeyValues();
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

void ItemDialog::setProfiles(const std::vector<Profile> &profiles, const QString &groupProfileId,
                             const QString &groupProfileName)
{
    const QSignalBlocker blocker(d->profile);
    d->groupProfile = groupProfileId;
    d->profileIds.clear();
    d->profile->clear();
    d->profile->addItem(tr("%1 (from the group)").arg(groupProfileName), QString());
    for (const Profile &profile : profiles) {
        d->profile->addItem(profile.name, profile.id);
        d->profileIds << profile.id;
    }
    // A profile that is not listed, because it was deleted or has not arrived
    // yet, is kept until another is chosen.
    const QString current = d->item.profile;
    if (d->profile->findData(current) < 0)
        d->profile->addItem(tr("Unknown profile, uses the group's"), current);
    d->profile->setCurrentIndex(d->profile->findData(current));
    // The form may offer values of the profile, and a link is read in the
    // group's profile.
    d->profileChosen();
    if (d->urlEdit && !d->urlEdit->text().trimmed().isEmpty())
        d->fillTypeList();
}

void ItemDialog::setProfile(const QString &id)
{
    const int index = d->profile->findData(id);
    if (index >= 0)
        d->profile->setCurrentIndex(index);
}

bool ItemDialog::showsProfile() const { return d->form->isRowVisible(d->profile); }

bool ItemDialog::showsWatch() const { return d->form->isRowVisible(d->watch); }

void ItemDialog::setWatch(bool watch) { d->watch->setChecked(watch); }

bool ItemDialog::showsPageKeys() const { return d->form->isRowVisible(d->pageKeys); }

QString ItemDialog::effectiveProfile() const { return d->effectiveProfile(); }

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
