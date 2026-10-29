#include "ui/GroupDialog.h"

#include "core/Edits.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace mirante {

namespace {

enum Column { KindColumn, NameColumn, BaseUrlColumn, DefaultColumn, ColumnCount };

const QString sharedProfile = QStringLiteral("shared");

// A host name as typed: trimmed and in lowercase. A pasted URL gives its host.
QString normaliseHost(const QString &text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.contains(QLatin1String("://")))
        return QUrl(trimmed).host().toLower();
    return trimmed.toLower();
}

bool isWebUrl(const QString &text)
{
    const QUrl url(text, QUrl::StrictMode);
    return url.isValid() && !url.host().isEmpty()
        && (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http"));
}

} // namespace

struct GroupDialog::Private
{
    struct Row
    {
        Connection connection;
        bool readOnly = false;   // no item type in this build uses its kind
    };

    GroupDialog *q = nullptr;
    Group group;
    QStringList kinds;                     // the kinds offered for new and edited connections
    std::vector<Row> rows;
    QMap<QString, QString> defaults;       // connection kind to connection id
    bool filling = false;                  // true while the table is rebuilt, to ignore its signals

    QLineEdit *name = nullptr;
    QTableWidget *connections = nullptr;
    QPushButton *addConnection = nullptr;
    QPushButton *removeConnection = nullptr;
    QLabel *noKinds = nullptr;             // shown when no item type uses a connection
    QListWidget *sharedSites = nullptr;
    QPushButton *removeSite = nullptr;
    QLabel *message = nullptr;             // why OK was refused

    int indexOf(const QString &connectionId) const
    {
        for (int i = 0; i < int(rows.size()); ++i) {
            if (rows[size_t(i)].connection.id == connectionId)
                return i;
        }
        return -1;
    }

    bool isDefault(const Connection &connection) const
    {
        return defaults.value(connection.kind) == connection.id;
    }

    // True if the default of kind is a listed connection of that kind.
    bool hasDefault(const QString &kind) const
    {
        const int index = indexOf(defaults.value(kind));
        return index >= 0 && rows[size_t(index)].connection.kind == kind;
    }

    void setKinds(const QStringList &offered)
    {
        kinds = offered;
        for (Row &row : rows)
            row.readOnly = !kinds.contains(row.connection.kind);
        fillTable();
    }

    QComboBox *kindBox(int index)
    {
        auto *box = new QComboBox;
        box->setObjectName(QStringLiteral("kind"));
        box->addItems(kinds);
        box->setCurrentText(rows[size_t(index)].connection.kind);
        const QString connectionId = rows[size_t(index)].connection.id;
        QObject::connect(box, &QComboBox::currentTextChanged, q,
                         [this, connectionId](const QString &kind) { changeKind(connectionId, kind); });
        return box;
    }

    void fillTable()
    {
        filling = true;
        const int current = connections->currentRow();
        connections->clearContents();
        connections->setRowCount(int(rows.size()));
        for (int i = 0; i < int(rows.size()); ++i) {
            const Row &row = rows[size_t(i)];
            const Connection &connection = row.connection;
            const Qt::ItemFlags fixed = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            const Qt::ItemFlags editable = row.readOnly ? fixed : fixed | Qt::ItemIsEditable;

            auto *kindItem = new QTableWidgetItem(row.readOnly ? connection.kind : QString());
            kindItem->setFlags(fixed);
            connections->setItem(i, KindColumn, kindItem);
            auto *nameItem = new QTableWidgetItem(connection.name);
            nameItem->setFlags(editable);
            connections->setItem(i, NameColumn, nameItem);
            auto *baseUrlItem = new QTableWidgetItem(connection.baseUrl);
            baseUrlItem->setFlags(editable);
            connections->setItem(i, BaseUrlColumn, baseUrlItem);
            auto *defaultItem = new QTableWidgetItem;
            defaultItem->setFlags(row.readOnly ? Qt::ItemIsSelectable : fixed | Qt::ItemIsUserCheckable);
            defaultItem->setCheckState(isDefault(connection) ? Qt::Checked : Qt::Unchecked);
            connections->setItem(i, DefaultColumn, defaultItem);

            if (row.readOnly) {
                const QString tip = GroupDialog::tr("No item type in this build uses the kind \"%1\". "
                                                    "The connection is kept as it is.")
                                        .arg(connection.kind);
                for (int column = 0; column < ColumnCount; ++column)
                    connections->item(i, column)->setToolTip(tip);
                connections->removeCellWidget(i, KindColumn);
            } else {
                connections->setCellWidget(i, KindColumn, kindBox(i));
            }
        }
        if (!rows.empty())
            connections->setCurrentCell(std::clamp(current, 0, int(rows.size()) - 1), NameColumn);
        filling = false;
        addConnection->setEnabled(!kinds.isEmpty());
        noKinds->setVisible(kinds.isEmpty());
        updateRemove();
    }

    // Sets the check boxes of the Default column from defaults.
    void showDefaults()
    {
        filling = true;
        for (int i = 0; i < int(rows.size()); ++i) {
            if (QTableWidgetItem *item = connections->item(i, DefaultColumn))
                item->setCheckState(isDefault(rows[size_t(i)].connection) ? Qt::Checked : Qt::Unchecked);
        }
        filling = false;
    }

    void updateRemove()
    {
        const int index = connections->currentRow();
        removeConnection->setEnabled(index >= 0 && index < int(rows.size()) && !rows[size_t(index)].readOnly);
    }

    void cellChanged(int index, int column)
    {
        if (filling || index < 0 || index >= int(rows.size()) || rows[size_t(index)].readOnly)
            return;
        QTableWidgetItem *item = connections->item(index, column);
        Connection &connection = rows[size_t(index)].connection;
        switch (column) {
        case NameColumn:
            connection.name = item->text().trimmed();
            break;
        case BaseUrlColumn:
            connection.baseUrl = item->text().trimmed();
            break;
        case DefaultColumn:
            // Checking a connection makes it the default of its kind, in
            // place of any other. Unchecking it leaves the kind without one.
            if (item->checkState() == Qt::Checked)
                defaults.insert(connection.kind, connection.id);
            else if (isDefault(connection))
                defaults.remove(connection.kind);
            showDefaults();
            break;
        default:
            break;
        }
    }

    void changeKind(const QString &connectionId, const QString &kind)
    {
        const int index = indexOf(connectionId);
        if (filling || index < 0 || kind.isEmpty())
            return;
        Connection &connection = rows[size_t(index)].connection;
        if (isDefault(connection))
            defaults.remove(connection.kind);
        connection.kind = kind;
        if (!hasDefault(kind))
            defaults.insert(kind, connection.id);
        showDefaults();
    }

    void add()
    {
        if (kinds.isEmpty())
            return;
        Row row;
        row.connection.id = edits::newId();
        row.connection.kind = kinds.front();
        // The first connection of a kind becomes its default.
        if (!hasDefault(row.connection.kind))
            defaults.insert(row.connection.kind, row.connection.id);
        rows.push_back(row);
        fillTable();
        const int index = int(rows.size()) - 1;
        connections->setCurrentCell(index, NameColumn);
        connections->editItem(connections->item(index, NameColumn));
    }

    void removeCurrent()
    {
        const int index = connections->currentRow();
        if (index < 0 || index >= int(rows.size()) || rows[size_t(index)].readOnly)
            return;
        const Connection &connection = rows[size_t(index)].connection;
        if (isDefault(connection))
            defaults.remove(connection.kind);
        rows.erase(rows.begin() + index);
        fillTable();
    }

    void addSite()
    {
        auto *item = new QListWidgetItem(sharedSites);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        sharedSites->setCurrentItem(item);
        sharedSites->editItem(item);
        removeSite->setEnabled(true);
    }

    void removeCurrentSite()
    {
        delete sharedSites->currentItem();
        removeSite->setEnabled(sharedSites->count() > 0);
    }

    // The hosts in the list, normalised, without blanks or repeats.
    QStringList hosts() const
    {
        QStringList result;
        for (int i = 0; i < sharedSites->count(); ++i) {
            const QString host = normaliseHost(sharedSites->item(i)->text());
            if (!host.isEmpty() && !result.contains(host))
                result << host;
        }
        return result;
    }
};

GroupDialog::GroupDialog(const Group &group, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->group = group;
    d->defaults = group.defaultConnections;
    for (const Connection &connection : group.connections)
        d->rows.push_back({connection, true});

    setWindowTitle(group.name.isEmpty() ? tr("New group") : tr("Edit group"));
    resize(640, 520);

    d->name = new QLineEdit(group.name, this);
    d->name->setObjectName(QStringLiteral("name"));

    // Connections.
    d->connections = new QTableWidget(0, ColumnCount, this);
    d->connections->setObjectName(QStringLiteral("connections"));
    d->connections->setHorizontalHeaderLabels({tr("Kind"), tr("Name"), tr("Base URL"), tr("Default")});
    d->connections->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->connections->setSelectionMode(QAbstractItemView::SingleSelection);
    d->connections->verticalHeader()->hide();
    d->connections->horizontalHeader()->setSectionResizeMode(KindColumn, QHeaderView::ResizeToContents);
    d->connections->horizontalHeader()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    d->connections->horizontalHeader()->setSectionResizeMode(BaseUrlColumn, QHeaderView::Stretch);
    d->connections->horizontalHeader()->setSectionResizeMode(DefaultColumn, QHeaderView::ResizeToContents);

    d->addConnection = new QPushButton(tr("Add connection"), this);
    d->addConnection->setObjectName(QStringLiteral("addConnection"));
    d->removeConnection = new QPushButton(tr("Remove connection"), this);
    d->removeConnection->setObjectName(QStringLiteral("removeConnection"));
    d->noKinds = new QLabel(tr("No item type in this build uses a connection."), this);
    d->noKinds->setObjectName(QStringLiteral("noKinds"));
    d->noKinds->setWordWrap(true);

    auto *connectionButtons = new QHBoxLayout;
    connectionButtons->addWidget(d->addConnection);
    connectionButtons->addWidget(d->removeConnection);
    connectionButtons->addStretch(1);

    auto *connectionHelp = new QLabel(
        tr("Items use these to reach a service. One connection of each kind can be the group default."), this);
    connectionHelp->setWordWrap(true);

    auto *connectionBox = new QGroupBox(tr("Connections"), this);
    auto *connectionLayout = new QVBoxLayout(connectionBox);
    connectionLayout->addWidget(connectionHelp);
    connectionLayout->addWidget(d->connections, 1);
    connectionLayout->addWidget(d->noKinds);
    connectionLayout->addLayout(connectionButtons);

    // Sites in the shared profile.
    d->sharedSites = new QListWidget(this);
    d->sharedSites->setObjectName(QStringLiteral("sharedSites"));
    for (auto it = group.profileOverrides.begin(); it != group.profileOverrides.end(); ++it) {
        if (it.value().toString() != sharedProfile)
            continue;
        auto *item = new QListWidgetItem(it.key(), d->sharedSites);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }

    auto *addSite = new QPushButton(tr("Add site"), this);
    addSite->setObjectName(QStringLiteral("addSite"));
    d->removeSite = new QPushButton(tr("Remove site"), this);
    d->removeSite->setObjectName(QStringLiteral("removeSite"));
    d->removeSite->setEnabled(d->sharedSites->count() > 0);

    auto *siteButtons = new QHBoxLayout;
    siteButtons->addWidget(addSite);
    siteButtons->addWidget(d->removeSite);
    siteButtons->addStretch(1);

    auto *siteHelp = new QLabel(tr("These sites share one login across all groups, such as acme.atlassian.net. "
                                   "Other sites keep a login for each group. Each module still has its own login."),
                                this);
    siteHelp->setWordWrap(true);

    auto *siteBox = new QGroupBox(tr("Sites in the shared profile"), this);
    auto *siteLayout = new QVBoxLayout(siteBox);
    siteLayout->addWidget(siteHelp);
    siteLayout->addWidget(d->sharedSites, 1);
    siteLayout->addLayout(siteButtons);

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *form = new QFormLayout;
    form->addRow(tr("Name:"), d->name);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &GroupDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(d->addConnection, &QPushButton::clicked, this, [this] { d->add(); });
    connect(d->removeConnection, &QPushButton::clicked, this, [this] { d->removeCurrent(); });
    connect(d->connections, &QTableWidget::cellChanged, this,
            [this](int row, int column) { d->cellChanged(row, column); });
    connect(d->connections, &QTableWidget::currentCellChanged, this, [this] { d->updateRemove(); });
    connect(addSite, &QPushButton::clicked, this, [this] { d->addSite(); });
    connect(d->removeSite, &QPushButton::clicked, this, [this] { d->removeCurrentSite(); });

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(connectionBox, 3);
    layout->addWidget(siteBox, 2);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    // Until setConnectionKinds() is called, every connection is read-only.
    d->setKinds({});
    d->name->setFocus();
}

GroupDialog::~GroupDialog()
{
    // The table outlives d, so its signals must not reach it.
    d->connections->disconnect(this);
}

Group GroupDialog::value() const
{
    Group group = d->group;
    group.name = d->name->text().trimmed();

    group.connections.clear();
    for (const Private::Row &row : d->rows)
        group.connections.push_back(row.connection);
    group.defaultConnections = d->defaults;

    // Hosts sent to the shared profile are replaced. Overrides with any other
    // value are kept as they are.
    for (const QString &host : group.profileOverrides.keys()) {
        if (group.profileOverrides.value(host).toString() == sharedProfile)
            group.profileOverrides.remove(host);
    }
    for (const QString &host : d->hosts())
        group.profileOverrides.insert(host, sharedProfile);
    return group;
}

void GroupDialog::setConnectionKinds(const QStringList &kinds) { d->setKinds(kinds); }
void GroupDialog::setName(const QString &name) { d->name->setText(name); }

void GroupDialog::accept()
{
    QString problem;
    QWidget *focus = nullptr;
    const auto editable = [](const Private::Row &row) { return !row.readOnly; };
    if (d->name->text().trimmed().isEmpty()) {
        problem = tr("Enter a name.");
        focus = d->name;
    } else if (std::any_of(d->rows.begin(), d->rows.end(), [&](const Private::Row &row) {
                   return editable(row) && row.connection.name.isEmpty();
               })) {
        problem = tr("Enter a name for every connection.");
        focus = d->connections;
    } else if (std::any_of(d->rows.begin(), d->rows.end(), [&](const Private::Row &row) {
                   return editable(row) && !isWebUrl(row.connection.baseUrl);
               })) {
        problem = tr("Enter a base URL for every connection, starting with https:// or http://.");
        focus = d->connections;
    }

    if (!problem.isEmpty()) {
        d->message->setText(problem);
        d->message->show();
        focus->setFocus();
        return;
    }
    QDialog::accept();
}

} // namespace mirante
