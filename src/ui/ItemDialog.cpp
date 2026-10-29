#include "ui/ItemDialog.h"

#include "items/FieldValidation.h"
#include "items/ItemTypeRegistry.h"
#include "ui/FieldForm.h"

#include <mirante/ItemType.h>

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

namespace mirante {

struct ItemDialog::Private
{
    ItemDialog *q = nullptr;
    const ItemTypeRegistry *types = nullptr;
    EditorContext context;
    Item item;
    QComboBox *type = nullptr;          // only when creating
    QLineEdit *name = nullptr;
    QFormLayout *form = nullptr;
    QComboBox *webProfile = nullptr;    // shown for types whose views are web views
    QVBoxLayout *editorBox = nullptr;
    ItemEditor *editor = nullptr;       // null for an item of a missing module
    QLabel *message = nullptr;          // why OK was refused

    // Replaces the editor with one for the type of item, showing config.
    void makeEditor(const QJsonObject &config)
    {
        delete editor;
        editor = nullptr;
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

    void typeChosen()
    {
        const QString id = type->currentData().toString();
        if (id == item.type)
            return;
        item.type = id;
        message->hide();
        makeEditor(defaultConfig(types->type(id).fields()));
        showWebProfile();
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

    const std::vector<const ItemType *> registered = types.types();
    QJsonObject config = item.config;
    if (creating) {
        if (d->item.type.isEmpty() && !registered.empty())
            d->item.type = registered.front()->id();
        if (config.isEmpty())
            config = defaultConfig(types.type(d->item.type).fields());
    }

    setWindowTitle(creating ? tr("New item") : tr("Edit item"));
    resize(480, 360);

    auto *form = new QFormLayout;
    d->form = form;
    if (creating) {
        d->type = new QComboBox(this);
        d->type->setObjectName(QStringLiteral("type"));
        for (const ItemType *type : registered)
            d->type->addItem(type->icon(), type->name(), type->id());
        d->type->setCurrentIndex(d->type->findData(d->item.type));
        form->addRow(tr("Type:"), d->type);
    } else {
        auto *typeName = new QLabel(types.type(d->item.type).name(), this);
        typeName->setObjectName(QStringLiteral("typeName"));
        form->addRow(tr("Type:"), typeName);
    }

    d->name = new QLineEdit(item.name, this);
    d->name->setObjectName(QStringLiteral("name"));
    form->addRow(tr("Name:"), d->name);

    d->webProfile = new QComboBox(this);
    d->webProfile->setObjectName(QStringLiteral("webProfile"));
    d->webProfile->setToolTip(tr("The item can read the logins and data of every site in its profile."));
    form->addRow(tr("Web profile:"), d->webProfile);

    d->editorBox = new QVBoxLayout;
    d->editorBox->setContentsMargins(0, 0, 0, 0);

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ItemDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(d->editorBox, 1);
    if (!types.contains(d->item.type)) {
        auto *missing = new QLabel(
            tr("The module of this item type is not available, so only the name can be changed. "
               "The rest of the item is kept."),
            this);
        missing->setObjectName(QStringLiteral("missing"));
        missing->setWordWrap(true);
        layout->addWidget(missing);
    }
    layout->addStretch(0);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    d->makeEditor(config);
    setWebProfiles({}, tr("Default"));
    d->showWebProfile();
    if (d->type)
        connect(d->type, &QComboBox::currentIndexChanged, this, [this] { d->typeChosen(); });

    d->name->setFocus();
}

ItemDialog::~ItemDialog()
{
    // The combo box outlives d, so its signals must not reach it.
    if (d->type)
        d->type->disconnect(this);
}

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
    if (!d->type)
        return;
    const int index = d->type->findData(typeId);
    if (index >= 0)
        d->type->setCurrentIndex(index);
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
