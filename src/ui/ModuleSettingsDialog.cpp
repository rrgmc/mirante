#include "ui/ModuleSettingsDialog.h"

#include "ui/FieldForm.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace mirante {

struct ModuleSettingsDialog::Private
{
    std::function<QJsonObject(const QString &)> stored;
    QComboBox *profile = nullptr;
    ItemEditor *editor = nullptr;
    QLabel *message = nullptr;   // why OK or a change of profile was refused
    QString shown;               // the profile whose values the editor shows
    // For each profile shown so far: the editor's values right after loading
    // them, and the values as last edited.
    std::map<QString, QJsonObject> loaded;
    std::map<QString, QJsonObject> edited;
    bool switching = false;      // true while the profile list is set back

    void show(const QString &profileId)
    {
        shown = profileId;
        const auto found = edited.find(profileId);
        if (found != edited.end()) {
            editor->setConfig(found->second);
            return;
        }
        editor->setConfig(stored ? stored(profileId) : QJsonObject());
        // What the editor gives for unchanged values, so a profile that was
        // only looked at is not written.
        loaded[profileId] = editor->config();
    }

    bool showProblems()
    {
        const QStringList problems = editor->problems();
        message->setText(problems.join(QLatin1Char('\n')));
        message->setVisible(!problems.isEmpty());
        return problems.isEmpty();
    }
};

ModuleSettingsDialog::ModuleSettingsDialog(const QString &moduleName, const SettingsScreen &screen,
                                           const std::vector<Profile> &profiles,
                                           std::function<QJsonObject(const QString &)> stored, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->stored = std::move(stored);
    setWindowTitle(tr("%1: %2").arg(moduleName, screen.name()));
    resize(480, 400);

    d->profile = new QComboBox(this);
    d->profile->setObjectName(QStringLiteral("profile"));
    for (const Profile &profile : profiles)
        d->profile->addItem(profile.name, profile.id);
    if (d->profile->findData(kDefaultProfile) < 0)
        d->profile->insertItem(0, tr("Default"), kDefaultProfile);
    d->profile->setCurrentIndex(d->profile->findData(kDefaultProfile));

    // Each profile has its own values; items use those of their profile.
    auto *note = new QLabel(tr("Each profile has its own settings. Items use the settings of their profile. The "
                               "settings are saved in the workspace and shared with it."),
                            this);
    note->setObjectName(QStringLiteral("note"));
    note->setWordWrap(true);

    d->editor = screen.createEditor(this);
    if (!d->editor)
        d->editor = new FieldForm(screen.fields(), EditorContext(), this);
    d->editor->setObjectName(QStringLiteral("editor"));
    d->show(kDefaultProfile);

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ModuleSettingsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(d->profile, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (d->switching)
            return;
        const QString id = d->profile->itemData(index).toString();
        if (!setProfile(id)) {
            d->switching = true;
            d->profile->setCurrentIndex(d->profile->findData(d->shown));
            d->switching = false;
        }
    });

    auto *profileRow = new QFormLayout;
    profileRow->addRow(tr("Profile:"), d->profile);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(profileRow);
    layout->addWidget(note);
    layout->addWidget(d->editor, 1);
    layout->addWidget(d->message);
    layout->addWidget(buttons);
}

ModuleSettingsDialog::~ModuleSettingsDialog()
{
    // The list outlives d, so its signals must not reach it.
    d->profile->disconnect(this);
}

QString ModuleSettingsDialog::profileId() const { return d->shown; }

bool ModuleSettingsDialog::setProfile(const QString &profileId)
{
    if (profileId == d->shown)
        return true;
    const int index = d->profile->findData(profileId);
    if (index < 0 || !d->showProblems())
        return false;
    d->edited[d->shown] = d->editor->config();
    d->show(profileId);
    if (d->profile->currentIndex() != index) {
        d->switching = true;
        d->profile->setCurrentIndex(index);
        d->switching = false;
    }
    return true;
}

std::map<QString, QJsonObject> ModuleSettingsDialog::values() const
{
    std::map<QString, QJsonObject> edited = d->edited;
    edited[d->shown] = d->editor->config();
    std::map<QString, QJsonObject> changed;
    for (const auto &[id, values] : edited) {
        const auto loaded = d->loaded.find(id);
        if (loaded == d->loaded.end() || loaded->second != values)
            changed.emplace(id, values);
    }
    return changed;
}

ItemEditor *ModuleSettingsDialog::editor() const { return d->editor; }

void ModuleSettingsDialog::accept()
{
    if (!d->showProblems())
        return;
    QDialog::accept();
}

} // namespace mirante
