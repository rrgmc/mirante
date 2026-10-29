#include "ui/ConnectionEditor.h"

#include "core/Edits.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace mirante {

namespace {

enum Column { KindColumn, NameColumn, BaseUrlColumn, DefaultColumn, ColumnCount };

bool isWebUrl(const QString &text)
{
    const QUrl url(text, QUrl::StrictMode);
    return url.isValid() && !url.host().isEmpty()
        && (url.scheme() == QLatin1String("https") || url.scheme() == QLatin1String("http"));
}

} // namespace

struct ConnectionEditor::Private
{
    struct Row
    {
        Connection connection;
        bool readOnly = false;   // no item type in this build uses its kind
    };

    ConnectionEditor *q = nullptr;
    QStringList kinds;                     // the kinds offered for new and edited connections
    std::vector<Row> rows;
    QMap<QString, QString> defaults;       // connection kind to connection id
    bool filling = false;                  // true while the table is rebuilt, to ignore its signals

    QTableWidget *table = nullptr;
    QPushButton *addButton = nullptr;
    QPushButton *removeButton = nullptr;
    QLabel *noKinds = nullptr;             // shown when no item type uses a connection

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
        const int current = table->currentRow();
        table->clearContents();
        table->setRowCount(int(rows.size()));
        for (int i = 0; i < int(rows.size()); ++i) {
            const Row &row = rows[size_t(i)];
            const Connection &connection = row.connection;
            const Qt::ItemFlags fixed = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            const Qt::ItemFlags editable = row.readOnly ? fixed : fixed | Qt::ItemIsEditable;

            auto *kindItem = new QTableWidgetItem(row.readOnly ? connection.kind : QString());
            kindItem->setFlags(fixed);
            table->setItem(i, KindColumn, kindItem);
            auto *nameItem = new QTableWidgetItem(connection.name);
            nameItem->setFlags(editable);
            table->setItem(i, NameColumn, nameItem);
            auto *baseUrlItem = new QTableWidgetItem(connection.baseUrl);
            baseUrlItem->setFlags(editable);
            table->setItem(i, BaseUrlColumn, baseUrlItem);
            auto *defaultItem = new QTableWidgetItem;
            defaultItem->setFlags(row.readOnly ? Qt::ItemIsSelectable : fixed | Qt::ItemIsUserCheckable);
            defaultItem->setCheckState(isDefault(connection) ? Qt::Checked : Qt::Unchecked);
            table->setItem(i, DefaultColumn, defaultItem);

            if (row.readOnly) {
                const QString tip = ConnectionEditor::tr("No item type in this build uses the kind \"%1\". "
                                                         "The connection is kept as it is.")
                                        .arg(connection.kind);
                for (int column = 0; column < ColumnCount; ++column)
                    table->item(i, column)->setToolTip(tip);
                table->removeCellWidget(i, KindColumn);
            } else {
                table->setCellWidget(i, KindColumn, kindBox(i));
            }
        }
        if (!rows.empty())
            table->setCurrentCell(std::clamp(current, 0, int(rows.size()) - 1), NameColumn);
        filling = false;
        addButton->setEnabled(!kinds.isEmpty());
        noKinds->setVisible(kinds.isEmpty());
        updateRemove();
    }

    // Sets the check boxes of the Default column from defaults.
    void showDefaults()
    {
        filling = true;
        for (int i = 0; i < int(rows.size()); ++i) {
            if (QTableWidgetItem *item = table->item(i, DefaultColumn))
                item->setCheckState(isDefault(rows[size_t(i)].connection) ? Qt::Checked : Qt::Unchecked);
        }
        filling = false;
    }

    void updateRemove()
    {
        const int index = table->currentRow();
        removeButton->setEnabled(index >= 0 && index < int(rows.size()) && !rows[size_t(index)].readOnly);
    }

    void cellChanged(int index, int column)
    {
        if (filling || index < 0 || index >= int(rows.size()) || rows[size_t(index)].readOnly)
            return;
        QTableWidgetItem *item = table->item(index, column);
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
};

ConnectionEditor::ConnectionEditor(const std::vector<Connection> &connections,
                                   const QMap<QString, QString> &defaults, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->defaults = defaults;
    for (const Connection &connection : connections)
        d->rows.push_back({connection, true});

    d->table = new QTableWidget(0, ColumnCount, this);
    d->table->setObjectName(QStringLiteral("connections"));
    d->table->setHorizontalHeaderLabels({tr("Kind"), tr("Name"), tr("Base URL"), tr("Default")});
    d->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->table->setSelectionMode(QAbstractItemView::SingleSelection);
    d->table->verticalHeader()->hide();
    d->table->horizontalHeader()->setSectionResizeMode(KindColumn, QHeaderView::ResizeToContents);
    d->table->horizontalHeader()->setSectionResizeMode(NameColumn, QHeaderView::Stretch);
    d->table->horizontalHeader()->setSectionResizeMode(BaseUrlColumn, QHeaderView::Stretch);
    d->table->horizontalHeader()->setSectionResizeMode(DefaultColumn, QHeaderView::ResizeToContents);

    d->addButton = new QPushButton(tr("Add connection"), this);
    d->addButton->setObjectName(QStringLiteral("addConnection"));
    d->removeButton = new QPushButton(tr("Remove connection"), this);
    d->removeButton->setObjectName(QStringLiteral("removeConnection"));
    d->noKinds = new QLabel(tr("No item type in this build uses a connection."), this);
    d->noKinds->setObjectName(QStringLiteral("noKinds"));
    d->noKinds->setWordWrap(true);

    auto *buttons = new QHBoxLayout;
    buttons->addWidget(d->addButton);
    buttons->addWidget(d->removeButton);
    buttons->addStretch(1);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(d->table, 1);
    layout->addWidget(d->noKinds);
    layout->addLayout(buttons);

    connect(d->addButton, &QPushButton::clicked, this, &ConnectionEditor::addConnection);
    connect(d->removeButton, &QPushButton::clicked, this, &ConnectionEditor::removeCurrent);
    connect(d->table, &QTableWidget::cellChanged, this, [this](int row, int column) { d->cellChanged(row, column); });
    connect(d->table, &QTableWidget::currentCellChanged, this, [this] { d->updateRemove(); });

    // Until setConnectionKinds() is called, every connection is read-only.
    d->setKinds({});
}

ConnectionEditor::~ConnectionEditor()
{
    // The table outlives d, so its signals must not reach it.
    d->table->disconnect(this);
}

void ConnectionEditor::setConnectionKinds(const QStringList &kinds) { d->setKinds(kinds); }

std::vector<Connection> ConnectionEditor::connections() const
{
    std::vector<Connection> result;
    for (const Private::Row &row : d->rows)
        result.push_back(row.connection);
    return result;
}

QMap<QString, QString> ConnectionEditor::defaults() const { return d->defaults; }

QString ConnectionEditor::problem() const
{
    const auto editable = [](const Private::Row &row) { return !row.readOnly; };
    if (std::any_of(d->rows.begin(), d->rows.end(), [&](const Private::Row &row) {
            return editable(row) && row.connection.name.isEmpty();
        }))
        return tr("Enter a name for every connection.");
    if (std::any_of(d->rows.begin(), d->rows.end(), [&](const Private::Row &row) {
            return editable(row) && !isWebUrl(row.connection.baseUrl);
        }))
        return tr("Enter a base URL for every connection, starting with https:// or http://.");
    return {};
}

QTableWidget *ConnectionEditor::table() const { return d->table; }

void ConnectionEditor::addConnection()
{
    if (d->kinds.isEmpty())
        return;
    Private::Row row;
    row.connection.id = edits::newId();
    row.connection.kind = d->kinds.front();
    // The first connection of a kind becomes its default.
    if (!d->hasDefault(row.connection.kind))
        d->defaults.insert(row.connection.kind, row.connection.id);
    d->rows.push_back(row);
    d->fillTable();
    const int index = int(d->rows.size()) - 1;
    d->table->setCurrentCell(index, NameColumn);
    d->table->editItem(d->table->item(index, NameColumn));
}

void ConnectionEditor::removeCurrent()
{
    const int index = d->table->currentRow();
    if (index < 0 || index >= int(d->rows.size()) || d->rows[size_t(index)].readOnly)
        return;
    const Connection &connection = d->rows[size_t(index)].connection;
    if (d->isDefault(connection))
        d->defaults.remove(connection.kind);
    d->rows.erase(d->rows.begin() + index);
    d->fillTable();
}

} // namespace mirante
