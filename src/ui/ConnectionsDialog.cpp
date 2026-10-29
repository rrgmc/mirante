#include "ui/ConnectionsDialog.h"

#include "ui/ConnectionEditor.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

namespace mirante {

struct ConnectionsDialog::Private
{
    Profile profile;
    ConnectionEditor *editor = nullptr;
    QLabel *message = nullptr;   // why OK was refused
};

ConnectionsDialog::ConnectionsDialog(const Profile &profile, const QString &profileName, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->profile = profile;
    setWindowTitle(tr("Connections of %1").arg(profileName));
    resize(640, 400);

    auto *help = new QLabel(tr("Items that use the profile \"%1\" reach services through these connections. One "
                               "connection of each kind can be the profile's default. Tokens are kept in this "
                               "device's keychain, not in the workspace.")
                                .arg(profileName),
                            this);
    help->setObjectName(QStringLiteral("help"));
    help->setWordWrap(true);

    d->editor = new ConnectionEditor(profile.connections, profile.defaultConnections, this);
    d->editor->setObjectName(QStringLiteral("connectionEditor"));

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ConnectionsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(help);
    layout->addWidget(d->editor, 1);
    layout->addWidget(d->message);
    layout->addWidget(buttons);
}

ConnectionsDialog::~ConnectionsDialog() = default;

void ConnectionsDialog::setConnectionKinds(const QStringList &kinds) { d->editor->setConnectionKinds(kinds); }

Profile ConnectionsDialog::value() const
{
    Profile profile = d->profile;
    profile.connections = d->editor->connections();
    profile.defaultConnections = d->editor->defaults();
    return profile;
}

ConnectionEditor *ConnectionsDialog::editor() const { return d->editor; }

void ConnectionsDialog::accept()
{
    const QString problem = d->editor->problem();
    if (!problem.isEmpty()) {
        d->message->setText(problem);
        d->message->show();
        d->editor->table()->setFocus();
        return;
    }
    QDialog::accept();
}

} // namespace mirante
