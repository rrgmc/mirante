#include "ui/ProfileSettingsDialog.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

namespace mirante {

struct ProfileSettingsDialog::Private
{
    QVBoxLayout *sections = nullptr;
};

namespace {

// Text the user can select and copy, but not change.
QLineEdit *readOnlyField(const QString &text, const QString &objectName, QWidget *parent)
{
    auto *field = new QLineEdit(text, parent);
    field->setObjectName(objectName);
    field->setReadOnly(true);
    field->setCursorPosition(0);
    field->setMinimumWidth(360);
    return field;
}

// A folder's path with an Open button. A folder not made yet, because no page
// of the profile has loaded, cannot be opened.
QWidget *folderRow(const QString &path, const QString &objectName, QWidget *parent)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(readOnlyField(QDir::toNativeSeparators(path), objectName, row), 1);
    auto *open = new QPushButton(ProfileSettingsDialog::tr("Open"), row);
    open->setObjectName(objectName + QStringLiteral("Open"));
    const bool exists = !path.isEmpty() && QFileInfo(path).isDir();
    open->setEnabled(exists);
    open->setToolTip(exists ? ProfileSettingsDialog::tr("Show the folder in the file manager")
                            : ProfileSettingsDialog::tr("Made when a page of this profile first loads"));
    QObject::connect(open, &QPushButton::clicked, row,
                     [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(path)); });
    layout->addWidget(open);
    return row;
}

} // namespace

ProfileSettingsDialog::ProfileSettingsDialog(const QString &profileName, const ProfileStorage &storage, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    setObjectName(QStringLiteral("profileSettingsDialog"));
    setWindowTitle(tr("Profile Settings: %1").arg(profileName));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    d->sections = new QVBoxLayout;
    d->sections->setSpacing(12);
    layout->addLayout(d->sections);
    layout->addStretch(1);
    layout->addWidget(buttons);

    // Storage: on this device only, never in the workspace.
    QFormLayout *form = addSection(tr("Storage"));
    auto *note = new QLabel(tr("Kept on this device only, for this workspace. Other devices keep their own."), this);
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    form->addRow(note);
    form->addRow(tr("Web data"), folderRow(storage.webData, QStringLiteral("webDataPath"), this));
    form->addRow(tr("Cache"), folderRow(storage.cache, QStringLiteral("cachePath"), this));
    auto *tokens = readOnlyField(storage.tokenPrefix + QStringLiteral("…"), QStringLiteral("tokenPrefix"), this);
    tokens->setToolTip(tr("Tokens are in the system keychain, in entries whose names start with this."));
    form->addRow(tr("Tokens"), tokens);

    resize(600, sizeHint().height());
}

ProfileSettingsDialog::~ProfileSettingsDialog() = default;

QFormLayout *ProfileSettingsDialog::addSection(const QString &title)
{
    auto *heading = new QLabel(title, this);
    QFont font = heading->font();
    font.setBold(true);
    heading->setFont(font);

    auto *form = new QFormLayout;
    form->setContentsMargins(12, 0, 0, 0);

    auto *section = new QVBoxLayout;
    section->setSpacing(4);
    section->addWidget(heading);
    section->addLayout(form);
    d->sections->addLayout(section);
    return form;
}

} // namespace mirante
