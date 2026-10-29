#include "ui/GroupDialog.h"

#include "core/Edits.h"
#include "core/GroupColours.h"

#include <QApplication>
#include <QButtonGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIconEngine>
#include <QLabel>
#include <QPainter>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
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

// A colour swatch: a filled circle, with a ring in the text colour around it
// while chosen.
class SwatchIconEngine : public QIconEngine
{
public:
    explicit SwatchIconEngine(const QColor &colour) : m_colour(colour) {}

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode, QIcon::State state) override
    {
        const qreal side = qMin(rect.width(), rect.height());
        const QRectF outer(rect.x() + (rect.width() - side) / 2.0, rect.y() + (rect.height() - side) / 2.0,
                           side, side);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (state == QIcon::On) {
            painter->setPen(QPen(QApplication::palette().color(QPalette::WindowText), 1.5));
            painter->setBrush(Qt::NoBrush);
            painter->drawEllipse(outer.adjusted(1, 1, -1, -1));
        }
        painter->setPen(Qt::NoPen);
        painter->setBrush(m_colour);
        const qreal inset = side * 0.22;
        painter->drawEllipse(outer.adjusted(inset, inset, -inset, -inset));
        painter->restore();
    }

    QIconEngine *clone() const override { return new SwatchIconEngine(m_colour); }

private:
    QColor m_colour;
};

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
    QComboBox *webProfile = nullptr;
    QLabel *message = nullptr;             // why OK was refused

    // The colour. chosenColour is empty until one is chosen in the dialog.
    QString chosenColour;
    QButtonGroup *swatches = nullptr;      // the palette, then the custom swatch
    QToolButton *customSwatch = nullptr;   // a colour outside the palette; hidden without one
    QToolButton *otherColour = nullptr;    // "Other…", opens QColorDialog

    QString shownColour() const
    {
        return chosenColour.isEmpty() ? groupcolours::of(group) : chosenColour;
    }

    QToolButton *swatch(const QString &colour, const QString &objectName)
    {
        auto *button = new QToolButton(q);
        button->setObjectName(objectName);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIconSize(QSize(20, 20));
        button->setIcon(QIcon(new SwatchIconEngine(QColor(colour))));
        button->setToolTip(colour);
        button->setProperty("colour", colour);
        return button;
    }

    // Checks the swatch of the shown colour. A colour outside the palette
    // gets the custom swatch.
    void showColour()
    {
        const QString colour = shownColour();
        const bool custom = !groupcolours::palette().contains(colour);
        if (custom) {
            customSwatch->setIcon(QIcon(new SwatchIconEngine(QColor(colour))));
            customSwatch->setToolTip(colour);
            customSwatch->setProperty("colour", colour);
        }
        customSwatch->setVisible(custom);
        for (QAbstractButton *button : swatches->buttons()) {
            if (button->property("colour").toString() == colour && (button == customSwatch) == custom)
                button->setChecked(true);
        }
    }

    void chooseColour(const QString &colour)
    {
        if (!groupcolours::isValid(colour))
            return;
        chosenColour = colour.toLower();
        showColour();
    }

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

    // The colour: the palette, a swatch for a colour outside it, and "Other…".
    auto *colourRow = new QHBoxLayout;
    colourRow->setSpacing(2);
    d->swatches = new QButtonGroup(this);
    d->swatches->setExclusive(true);
    for (int i = 0; i < groupcolours::palette().size(); ++i) {
        QToolButton *button = d->swatch(groupcolours::palette().at(i), QStringLiteral("colourSwatch%1").arg(i));
        d->swatches->addButton(button);
        colourRow->addWidget(button);
    }
    d->customSwatch = d->swatch(groupcolours::palette().front(), QStringLiteral("customColour"));
    d->swatches->addButton(d->customSwatch);
    colourRow->addWidget(d->customSwatch);
    d->otherColour = new QToolButton(this);
    d->otherColour->setObjectName(QStringLiteral("otherColour"));
    d->otherColour->setText(tr("Other…"));
    d->otherColour->setAutoRaise(true);
    colourRow->addWidget(d->otherColour);
    colourRow->addStretch(1);
    connect(d->swatches, &QButtonGroup::buttonClicked, this,
            [this](QAbstractButton *button) { d->chooseColour(button->property("colour").toString()); });
    connect(d->otherColour, &QToolButton::clicked, this, [this] {
        const QColor colour = QColorDialog::getColor(QColor(d->shownColour()), this, tr("Group colour"));
        if (colour.isValid())
            d->chooseColour(colour.name());
    });
    d->showColour();
    auto *colourHelp = new QLabel(tr("The title bars of the group's task dashboards and its projects in the sidebar "
                                     "are tinted with this colour."),
                                  this);
    colourHelp->setWordWrap(true);

    // The web profile.
    d->webProfile = new QComboBox(this);
    d->webProfile->setObjectName(QStringLiteral("webProfile"));
    auto *profileHelp = new QLabel(tr("Web items of this group use this profile, unless an item chooses its own. "
                                      "Manage profiles with File > Web Profiles…."),
                                   this);
    profileHelp->setWordWrap(true);

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *form = new QFormLayout;
    form->addRow(tr("Name:"), d->name);
    form->addRow(tr("Colour:"), colourRow);
    form->addRow(QString(), colourHelp);
    form->addRow(tr("Web profile:"), d->webProfile);
    form->addRow(QString(), profileHelp);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &GroupDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(d->addConnection, &QPushButton::clicked, this, [this] { d->add(); });
    connect(d->removeConnection, &QPushButton::clicked, this, [this] { d->removeCurrent(); });
    connect(d->connections, &QTableWidget::cellChanged, this,
            [this](int row, int column) { d->cellChanged(row, column); });
    connect(d->connections, &QTableWidget::currentCellChanged, this, [this] { d->updateRemove(); });

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addWidget(connectionBox, 1);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    // Until setConnectionKinds() is called, every connection is read-only.
    d->setKinds({});
    // Until setWebProfiles() is called, only Default is offered.
    setWebProfiles({});
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

    // Default is stored as no profile.
    const QString profile = d->webProfile->currentData().toString();
    group.webProfile = profile == kDefaultWebProfile ? QString() : profile;

    if (!d->chosenColour.isEmpty())
        group.colour = d->chosenColour;
    return group;
}

QString GroupDialog::colour() const { return d->shownColour(); }
void GroupDialog::setColour(const QString &colour) { d->chooseColour(colour); }

void GroupDialog::setConnectionKinds(const QStringList &kinds) { d->setKinds(kinds); }
void GroupDialog::setName(const QString &name) { d->name->setText(name); }

void GroupDialog::setWebProfiles(const std::vector<WebProfile> &profiles)
{
    d->webProfile->clear();
    bool hasDefault = false;
    for (const WebProfile &profile : profiles) {
        d->webProfile->addItem(profile.name, profile.id);
        hasDefault = hasDefault || profile.id == kDefaultWebProfile;
    }
    if (!hasDefault)
        d->webProfile->insertItem(0, tr("Default"), kDefaultWebProfile);
    // A profile that is not listed, because it was deleted or has not arrived
    // yet, is kept until another is chosen.
    const QString current = d->group.webProfile.isEmpty() ? kDefaultWebProfile : d->group.webProfile;
    if (d->webProfile->findData(current) < 0)
        d->webProfile->addItem(tr("Unknown profile, uses Default"), current);
    d->webProfile->setCurrentIndex(d->webProfile->findData(current));
}

void GroupDialog::setWebProfile(const QString &id)
{
    const int index = d->webProfile->findData(id);
    if (index >= 0)
        d->webProfile->setCurrentIndex(index);
}

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
