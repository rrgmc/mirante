#include "ui/ProjectDialog.h"

#include "core/DeviceSettings.h"
#include "core/Edits.h"
#include "core/Model.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace mirante {

namespace {

enum Column { NameColumn, RemoteColumn, MainColumn, PathColumn, ColumnCount };

void selectData(QComboBox *combo, const QVariant &data)
{
    const int index = combo->findData(data);
    if (index >= 0)
        combo->setCurrentIndex(index);
}

} // namespace

struct ProjectDialog::Private
{
    struct Row
    {
        Folder folder;
        QString path;       // on this device; empty if not set
    };

    ProjectDialog *q = nullptr;
    Project project;
    std::vector<Row> rows;
    bool filling = false;   // true while the table is rebuilt, to ignore its signals

    QLineEdit *name = nullptr;
    QComboBox *group = nullptr;
    QComboBox *priority = nullptr;
    QTableWidget *folders = nullptr;
    QPushButton *removeFolder = nullptr;
    QLabel *message = nullptr;      // why OK was refused

    // Exactly one folder is main when there are any: the first main one, or
    // else the first folder.
    void fixMain()
    {
        bool found = false;
        for (Row &row : rows) {
            if (row.folder.main && !found)
                found = true;
            else
                row.folder.main = false;
        }
        if (!found && !rows.empty())
            rows.front().folder.main = true;
    }

    void setMain(int index)
    {
        for (int i = 0; i < int(rows.size()); ++i)
            rows[size_t(i)].folder.main = i == index;
    }

    QWidget *pathCell(int index)
    {
        auto *cell = new QWidget;
        auto *label = new QLabel(cell);
        label->setObjectName(QStringLiteral("path"));
        const QString &path = rows[size_t(index)].path;
        label->setText(path.isEmpty() ? ProjectDialog::tr("Folder not set on this device")
                                      : QDir::toNativeSeparators(path));
        label->setEnabled(!path.isEmpty());
        auto *choose = new QPushButton(ProjectDialog::tr("Choose…"), cell);
        choose->setObjectName(QStringLiteral("choose"));
        const QString folderId = rows[size_t(index)].folder.id;
        QObject::connect(choose, &QPushButton::clicked, q, [this, folderId] { choosePath(folderId); });
        auto *layout = new QHBoxLayout(cell);
        layout->setContentsMargins(4, 0, 4, 0);
        layout->addWidget(label, 1);
        layout->addWidget(choose);
        return cell;
    }

    void fillTable()
    {
        filling = true;
        const int current = folders->currentRow();
        folders->clearContents();
        folders->setRowCount(int(rows.size()));
        for (int i = 0; i < int(rows.size()); ++i) {
            const Folder &folder = rows[size_t(i)].folder;
            folders->setItem(i, NameColumn, new QTableWidgetItem(folder.name));
            folders->setItem(i, RemoteColumn, new QTableWidgetItem(folder.remoteUrl));
            auto *main = new QTableWidgetItem;
            main->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            main->setCheckState(folder.main ? Qt::Checked : Qt::Unchecked);
            folders->setItem(i, MainColumn, main);
            auto *path = new QTableWidgetItem;
            path->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
            folders->setItem(i, PathColumn, path);
            folders->setCellWidget(i, PathColumn, pathCell(i));
        }
        if (!rows.empty())
            folders->setCurrentCell(std::clamp(current, 0, int(rows.size()) - 1), NameColumn);
        removeFolder->setEnabled(!rows.empty());
        filling = false;
    }

    // Sets the check boxes of the Main column from rows.
    void showMain()
    {
        filling = true;
        for (int i = 0; i < int(rows.size()); ++i) {
            if (QTableWidgetItem *item = folders->item(i, MainColumn))
                item->setCheckState(rows[size_t(i)].folder.main ? Qt::Checked : Qt::Unchecked);
        }
        filling = false;
    }

    void cellChanged(int row, int column)
    {
        if (filling || row < 0 || row >= int(rows.size()))
            return;
        QTableWidgetItem *item = folders->item(row, column);
        Folder &folder = rows[size_t(row)].folder;
        switch (column) {
        case NameColumn:
            folder.name = item->text().trimmed();
            break;
        case RemoteColumn:
            folder.remoteUrl = item->text().trimmed();
            break;
        case MainColumn:
            // Checking a folder makes it the main one. Unchecking the main
            // folder is undone, since one folder must stay main.
            if (item->checkState() == Qt::Checked)
                setMain(row);
            showMain();
            break;
        default:
            break;
        }
    }

    void addFolder()
    {
        Row row;
        row.folder.id = edits::newId();
        row.folder.main = rows.empty();
        rows.push_back(row);
        fillTable();
        const int index = int(rows.size()) - 1;
        folders->setCurrentCell(index, NameColumn);
        folders->editItem(folders->item(index, NameColumn));
    }

    void removeCurrentFolder()
    {
        const int index = folders->currentRow();
        if (index < 0 || index >= int(rows.size()))
            return;
        rows.erase(rows.begin() + index);
        fixMain();
        fillTable();
    }

    void choosePath(const QString &folderId)
    {
        auto it = std::find_if(rows.begin(), rows.end(), [&](const Row &row) { return row.folder.id == folderId; });
        if (it == rows.end())
            return;
        const QString path = QFileDialog::getExistingDirectory(
            q, ProjectDialog::tr("Choose the folder \"%1\"").arg(it->folder.name), it->path);
        if (path.isEmpty())
            return;
        it->path = path;
        fillTable();
    }
};

ProjectDialog::ProjectDialog(const Project &project, const Model &model, const DeviceSettings &settings,
                             QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->project = project;
    for (const Folder &folder : project.folders)
        d->rows.push_back({folder, settings.folderPath(folder.id)});
    d->fixMain();

    setWindowTitle(project.name.isEmpty() ? tr("New project") : tr("Edit project"));
    resize(640, 400);

    d->name = new QLineEdit(project.name, this);
    d->name->setObjectName(QStringLiteral("name"));

    d->group = new QComboBox(this);
    d->group->setObjectName(QStringLiteral("group"));
    std::vector<const Group *> groups = model.groups();
    std::sort(groups.begin(), groups.end(), [](const Group *a, const Group *b) {
        return a->order != b->order ? a->order < b->order : a->id < b->id;
    });
    for (const Group *group : groups)
        d->group->addItem(group->name, group->id);
    if (!project.groupId.isEmpty() && d->group->findData(project.groupId) < 0)
        d->group->addItem(tr("(unknown group)"), project.groupId);
    selectData(d->group, project.groupId);

    d->priority = new QComboBox(this);
    d->priority->setObjectName(QStringLiteral("priority"));
    d->priority->addItem(tr("Urgent"), int(Priority::Urgent));
    d->priority->addItem(tr("High"), int(Priority::High));
    d->priority->addItem(tr("Normal"), int(Priority::Normal));
    d->priority->addItem(tr("Low"), int(Priority::Low));
    selectData(d->priority, int(project.priority));

    d->folders = new QTableWidget(0, ColumnCount, this);
    d->folders->setObjectName(QStringLiteral("folders"));
    d->folders->setHorizontalHeaderLabels({tr("Name"), tr("Remote URL"), tr("Main"), tr("Path on this device")});
    d->folders->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->folders->setSelectionMode(QAbstractItemView::SingleSelection);
    d->folders->verticalHeader()->hide();
    d->folders->horizontalHeader()->setSectionResizeMode(NameColumn, QHeaderView::ResizeToContents);
    d->folders->horizontalHeader()->setSectionResizeMode(RemoteColumn, QHeaderView::Stretch);
    d->folders->horizontalHeader()->setSectionResizeMode(MainColumn, QHeaderView::ResizeToContents);
    d->folders->horizontalHeader()->setSectionResizeMode(PathColumn, QHeaderView::Stretch);

    auto *addFolder = new QPushButton(tr("Add folder"), this);
    addFolder->setObjectName(QStringLiteral("addFolder"));
    d->removeFolder = new QPushButton(tr("Remove folder"), this);
    d->removeFolder->setObjectName(QStringLiteral("removeFolder"));

    d->message = new QLabel(this);
    d->message->setObjectName(QStringLiteral("message"));
    d->message->setWordWrap(true);
    d->message->hide();

    auto *folderButtons = new QHBoxLayout;
    folderButtons->addWidget(addFolder);
    folderButtons->addWidget(d->removeFolder);
    folderButtons->addStretch(1);

    auto *folderBox = new QVBoxLayout;
    folderBox->addWidget(d->folders, 1);
    folderBox->addLayout(folderButtons);

    auto *form = new QFormLayout;
    form->addRow(tr("Name:"), d->name);
    form->addRow(tr("Group:"), d->group);
    form->addRow(tr("Priority:"), d->priority);
    form->addRow(tr("Folders:"), folderBox);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProjectDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(addFolder, &QPushButton::clicked, this, [this] { d->addFolder(); });
    connect(d->removeFolder, &QPushButton::clicked, this, [this] { d->removeCurrentFolder(); });
    connect(d->folders, &QTableWidget::cellChanged, this, [this](int row, int column) { d->cellChanged(row, column); });

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form, 1);
    layout->addWidget(d->message);
    layout->addWidget(buttons);

    d->fillTable();
    d->name->setFocus();
}

ProjectDialog::~ProjectDialog()
{
    // The table outlives d, so its signals must not reach it.
    d->folders->disconnect(this);
}

Project ProjectDialog::value() const
{
    Project project = d->project;
    project.name = d->name->text().trimmed();
    const QVariant group = d->group->currentData();
    if (group.isValid())
        project.groupId = group.toString();
    project.priority = Priority(d->priority->currentData().toInt());
    project.folders.clear();
    for (const Private::Row &row : d->rows)
        project.folders.push_back(row.folder);
    return project;
}

QMap<QString, QString> ProjectDialog::folderPaths() const
{
    QMap<QString, QString> paths;
    for (const Private::Row &row : d->rows)
        paths.insert(row.folder.id, row.path);
    return paths;
}

void ProjectDialog::setName(const QString &name) { d->name->setText(name); }
void ProjectDialog::setGroupId(const QString &groupId) { selectData(d->group, groupId); }
void ProjectDialog::setPriority(Priority priority) { selectData(d->priority, int(priority)); }

void ProjectDialog::setFolders(const std::vector<Folder> &folders, const QMap<QString, QString> &paths)
{
    d->rows.clear();
    for (const Folder &folder : folders)
        d->rows.push_back({folder, paths.value(folder.id)});
    d->fixMain();
    d->fillTable();
}

void ProjectDialog::accept()
{
    QString problem;
    if (d->name->text().trimmed().isEmpty())
        problem = tr("Enter a name.");
    else if (std::any_of(d->rows.begin(), d->rows.end(), [](const Private::Row &row) { return row.folder.name.isEmpty(); }))
        problem = tr("Enter a name for every folder.");

    if (!problem.isEmpty()) {
        d->message->setText(problem);
        d->message->show();
        return;
    }
    QDialog::accept();
}

} // namespace mirante
