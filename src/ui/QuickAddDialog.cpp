#include "ui/QuickAddDialog.h"

#include "core/Model.h"
#include "core/Profiles.h"
#include "core/Sorting.h"
#include "items/ItemTypeRegistry.h"
#include "items/QuickAddRun.h"

#include <mirante/ItemType.h>

#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>
#include <vector>

namespace mirante {

namespace {

// The role of an item row that holds its index in Private::items. Rows
// without it, such as the "Related" heading, are not items.
constexpr int IndexRole = Qt::UserRole + 1;

bool groupBefore(const Group *a, const Group *b)
{
    if (a->order != b->order)
        return a->order < b->order;
    if (const int c = QString::compare(a->name, b->name, Qt::CaseInsensitive); c != 0)
        return c < 0;
    return a->id < b->id;
}

// The value of a project match as the user would write it.
QString matchText(const QJsonValue &value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toDouble());
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    return {};
}

} // namespace

struct QuickAddDialog::Private
{
    QuickAddDialog *q = nullptr;
    Model &model;
    const ItemTypeRegistry &types;
    QuickAddRun &run;

    QLineEdit *text = nullptr;
    QComboBox *profile = nullptr;
    QLabel *status = nullptr;
    QLabel *readAsLabel = nullptr;
    QComboBox *readAs = nullptr;
    QLineEdit *title = nullptr;
    QComboBox *project = nullptr;
    QLabel *projectHint = nullptr;
    QListWidget *itemList = nullptr;
    QLabel *noProjects = nullptr;
    QPushButton *create = nullptr;
    QFormLayout *form = nullptr;

    std::vector<QuickAddRun::Proposal> proposals;
    int chosen = -1;                    // the proposal shown, or -1
    std::vector<QuickAddItem> items;    // every row's item, by IndexRole
    bool reading = false;
    bool finding = false;
    bool filling = false;               // set while the dialog changes its own widgets
    QString currentText;                // the text box, and its text before
    QString previousText;               // the last change, to tell a paste
    QString created;
    QString taskId;                     // the task the items go to, or empty for a new task
    QStringList added;

    Private(QuickAddDialog *q, Model &model, const ItemTypeRegistry &types, QuickAddRun &run)
        : q(q)
        , model(model)
        , types(types)
        , run(run)
    {
    }

    QString profileId() const { return profile->currentData().toString(); }
    bool intoTask() const { return !taskId.isEmpty(); }

    QString projectId() const
    {
        if (intoTask()) {
            const Task *task = model.task(taskId);
            return task ? task->projectId : QString();
        }
        return project->currentData().toString();
    }

    // The profiles, as the item dialog lists them: the default first, then
    // the others by name.
    void fillProfiles(const QString &start)
    {
        for (const Profile &p : profiles::all(model))
            profile->addItem(profiles::listName(model, p), p.id);
        int index = profile->findData(start);
        if (index < 0)
            index = profile->findData(model.defaultProfile());
        profile->setCurrentIndex(std::max(index, 0));
    }

    // Every group in sidebar order, each a heading that cannot be chosen,
    // followed by its projects outside the trash. Returns the number of
    // projects.
    int fillProjects()
    {
        auto *rows = qobject_cast<QStandardItemModel *>(project->model());
        std::vector<const Group *> groups = model.groups();
        std::sort(groups.begin(), groups.end(), groupBefore);
        int count = 0;
        for (const Group *group : groups) {
            const std::vector<const Project *> projects
                = sortedProjects(model.projectsOfGroup(group->id), SortFilter());
            if (projects.empty())
                continue;
            project->addItem(group->name);
            if (rows)
                rows->item(project->count() - 1)->setFlags(Qt::NoItemFlags);
            for (const Project *p : projects) {
                project->addItem(QStringLiteral("    ") + p->name, p->id);
                ++count;
            }
        }
        project->setCurrentIndex(-1);
        return count;
    }

    void setStatus()
    {
        if (reading)
            status->setText(QuickAddDialog::tr("Reading…"));
        else if (!text->text().trimmed().isEmpty() && proposals.empty())
            status->setText(QuickAddDialog::tr("Mirante does not recognise this text."));
        else if (finding)
            status->setText(QuickAddDialog::tr("Looking for related items…"));
        else
            status->clear();
        status->setVisible(!status->text().isEmpty());
    }

    void setDetailsVisible(bool visible)
    {
        // A task has its title and project already.
        const bool details = visible && !intoTask();
        form->setRowVisible(title, details);
        form->setRowVisible(project, details);
        form->setRowVisible(projectHint, details && !projectHint->text().isEmpty());
        form->setRowVisible(itemList, visible);
    }

    void updateCreate()
    {
        if (intoTask())
            create->setEnabled(chosen >= 0 && model.task(taskId));
        else
            create->setEnabled(chosen >= 0 && !title->text().trimmed().isEmpty() && !projectId().isEmpty());
    }

    void read()
    {
        const QString pasted = text->text().trimmed();
        proposals.clear();
        chosen = -1;
        clearForm();
        if (pasted.isEmpty()) {
            run.cancel();
            reading = finding = false;
            setStatus();
            return;
        }
        reading = true;
        finding = false;
        setStatus();
        run.propose(pasted, profileId());
    }

    void clearForm()
    {
        filling = true;
        readAs->clear();
        form->setRowVisible(readAs, false);
        title->clear();
        project->setCurrentIndex(-1);
        projectHint->clear();
        itemList->clear();
        items.clear();
        filling = false;
        setDetailsVisible(false);
        updateCreate();
    }

    void addItem(const QuickAddItem &item)
    {
        auto *row = new QListWidgetItem(types.type(item.typeId).icon(), item.name);
        row->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        row->setCheckState(Qt::Checked);
        row->setData(IndexRole, int(items.size()));
        items.push_back(item);
        itemList->addItem(row);
    }

    // Drops the finders' items and their heading.
    void removeRelated()
    {
        for (int i = itemList->count() - 1; i >= 0; --i) {
            const QVariant index = itemList->item(i)->data(IndexRole);
            if (!index.isValid() || index.toInt() >= int(proposals[size_t(chosen)].proposal.items.size()))
                delete itemList->takeItem(i);
        }
        items.resize(proposals[size_t(chosen)].proposal.items.size());
    }

    // Shows a proposal: its title, project and items. Then looks for
    // related items if a project is chosen, at once for a task.
    void showProposal(int index)
    {
        chosen = index;
        const QuickAddRun::Proposal &proposal = proposals[size_t(index)];
        filling = true;
        title->setText(proposal.proposal.title);
        itemList->clear();
        items.clear();
        for (const QuickAddItem &item : proposal.proposal.items)
            addItem(item);

        const QStringList matches = intoTask()
            ? QStringList()
            : QuickAddRun::matchProjects(model, proposal.moduleId, proposal.proposal.project);
        if (intoTask()) {
            projectHint->clear();
        } else if (matches.size() == 1) {
            project->setCurrentIndex(project->findData(matches.front()));
            projectHint->clear();
        } else {
            project->setCurrentIndex(-1);
            if (matches.size() > 1)
                projectHint->setText(QuickAddDialog::tr("Several projects match"));
            else if (proposal.proposal.project)
                projectHint->setText(QuickAddDialog::tr("No project matches %1")
                                         .arg(matchText(proposal.proposal.project->value)));
            else
                projectHint->clear();
        }
        filling = false;
        setDetailsVisible(true);
        updateCreate();
        findRelated();
    }

    void findRelated()
    {
        if (chosen < 0 || projectId().isEmpty()) {
            finding = false;
            setStatus();
            return;
        }
        finding = true;
        setStatus();
        run.findRelated(proposals[size_t(chosen)], title->text(), projectId(), profileId());
    }

    void onProposed(const QuickAddRun::Proposal &proposal)
    {
        proposals.push_back(proposal);
        filling = true;
        readAs->addItem(types.moduleName(proposal.moduleId));
        filling = false;
        form->setRowVisible(readAs, proposals.size() > 1);
        if (proposals.size() == 1)
            showProposal(0);
    }

    void onFound(const std::vector<QuickAddItem> &found)
    {
        if (chosen < 0 || found.empty())
            return;
        bool heading = false;
        for (int i = 0; i < itemList->count(); ++i)
            heading = heading || !itemList->item(i)->data(IndexRole).isValid();
        if (!heading) {
            auto *row = new QListWidgetItem(QuickAddDialog::tr("Related"));
            row->setFlags(Qt::NoItemFlags);
            QFont font = row->font();
            font.setBold(true);
            row->setFont(font);
            itemList->addItem(row);
        }
        for (const QuickAddItem &item : found)
            addItem(item);
    }

    void onProjectChosen()
    {
        if (filling || chosen < 0)
            return;
        projectHint->clear();
        form->setRowVisible(projectHint, false);
        removeRelated();
        updateCreate();
        findRelated();
    }

    // Create Task, or Add Items for a task.
    void accept()
    {
        if (!create->isEnabled())
            return;
        std::vector<QuickAddItem> chosenItems;
        for (int i = 0; i < itemList->count(); ++i) {
            const QListWidgetItem *row = itemList->item(i);
            const QVariant index = row->data(IndexRole);
            if (index.isValid() && row->checkState() == Qt::Checked)
                chosenItems.push_back(items[size_t(index.toInt())]);
        }
        run.cancel();
        if (intoTask()) {
            const QStringList ids = run.addToTask(taskId, chosenItems);
            if (!model.task(taskId))
                return;
            added = ids;
            emit q->itemsAdded(taskId, ids);
            q->accept();
            return;
        }
        const QString id = run.create(projectId(), title->text().trimmed(), chosenItems);
        if (id.isEmpty())
            return;
        created = id;
        emit q->taskCreated(id);
        q->accept();
    }
};

QuickAddDialog::QuickAddDialog(Model &model, const ItemTypeRegistry &types, QuickAddRun &run,
                               const QString &profileId, QWidget *parent)
    : QuickAddDialog(model, types, run, profileId, QString(), parent)
{
}

QuickAddDialog *QuickAddDialog::forTask(Model &model, const ItemTypeRegistry &types, QuickAddRun &run,
                                        const QString &taskId, QWidget *parent)
{
    // The profile of the task's group, else the workspace's default.
    QString profileId;
    const Task *task = model.task(taskId);
    const Project *project = task ? model.project(task->projectId) : nullptr;
    if (const Group *group = project ? model.group(project->groupId) : nullptr)
        profileId = profiles::groupDefault(model, *group);
    return new QuickAddDialog(model, types, run, profileId, taskId, parent);
}

QuickAddDialog::QuickAddDialog(Model &model, const ItemTypeRegistry &types, QuickAddRun &run,
                               const QString &profileId, const QString &taskId, QWidget *parent)
    : QDialog(parent)
    , d(std::make_unique<Private>(this, model, types, run))
{
    d->taskId = taskId;
    run.setTask(taskId);
    if (d->intoTask()) {
        const Task *task = model.task(taskId);
        setWindowTitle(tr("Add Items from Link: %1").arg(task ? task->title : QString()));
    } else {
        setWindowTitle(tr("New Task from Link"));
    }
    setObjectName(QStringLiteral("quickAddDialog"));

    d->text = new QLineEdit;
    d->text->setObjectName(QStringLiteral("text"));
    d->text->setPlaceholderText(tr("Paste a link or a key"));
    d->text->setClearButtonEnabled(true);
    d->profile = new QComboBox;
    d->profile->setObjectName(QStringLiteral("profile"));
    d->status = new QLabel;
    d->status->setObjectName(QStringLiteral("status"));
    d->status->setWordWrap(true);
    d->readAs = new QComboBox;
    d->readAs->setObjectName(QStringLiteral("readAs"));
    d->title = new QLineEdit;
    d->title->setObjectName(QStringLiteral("title"));
    d->project = new QComboBox;
    d->project->setObjectName(QStringLiteral("project"));
    d->project->setPlaceholderText(tr("Choose a project"));
    d->projectHint = new QLabel;
    d->projectHint->setObjectName(QStringLiteral("projectHint"));
    d->itemList = new QListWidget;
    d->itemList->setObjectName(QStringLiteral("items"));
    d->noProjects = new QLabel(tr("Create a project first."));
    d->noProjects->setObjectName(QStringLiteral("noProjects"));

    d->form = new QFormLayout;
    d->form->addRow(tr("Text"), d->text);
    d->form->addRow(tr("Profile"), d->profile);
    d->form->addRow(QString(), d->status);
    d->form->addRow(tr("Read as"), d->readAs);
    d->form->addRow(tr("Title"), d->title);
    d->form->addRow(tr("Project"), d->project);
    d->form->addRow(QString(), d->projectHint);
    d->form->addRow(tr("Items"), d->itemList);

    auto *buttons = new QDialogButtonBox;
    // An action, not an accept button: the button box would make an accept
    // button the default, and Return in the text box would press it.
    d->create = buttons->addButton(d->intoTask() ? tr("Add Items") : tr("Create Task"),
                                   QDialogButtonBox::ActionRole);
    d->create->setObjectName(QStringLiteral("create"));
    QPushButton *cancel = buttons->addButton(QDialogButtonBox::Cancel);
    cancel->setObjectName(QStringLiteral("cancel"));
    // Return in the text box reads it, and presses no button.
    d->create->setAutoDefault(false);
    cancel->setAutoDefault(false);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(d->noProjects);
    layout->addLayout(d->form);
    layout->addWidget(buttons);

    d->fillProfiles(profileId);
    // A task has a project; a new task needs one.
    const bool hasProjects = d->fillProjects() > 0 || d->intoTask();
    d->noProjects->setVisible(!hasProjects);
    d->create->setVisible(hasProjects);
    d->clearForm();
    d->setStatus();

    // Reading starts on Return, or when text is pasted rather than typed.
    connect(d->text, &QLineEdit::returnPressed, this, [this] { d->read(); });
    connect(d->text, &QLineEdit::textChanged, this, [this](const QString &now) {
        d->previousText = std::exchange(d->currentText, now);
    });
    connect(d->text, &QLineEdit::textEdited, this, [this](const QString &now) {
        // More than one character at once is a paste. An emptied box clears
        // the dialog. textChanged may come first, or after.
        const QString &before = d->currentText == now ? d->previousText : d->currentText;
        if (now.size() - before.size() > 1 || now.trimmed().isEmpty())
            d->read();
    });
    connect(d->profile, &QComboBox::currentIndexChanged, this, [this] {
        if (!d->text->text().trimmed().isEmpty())
            d->read();
    });
    connect(d->readAs, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!d->filling && index >= 0 && index < int(d->proposals.size()) && index != d->chosen)
            d->showProposal(index);
    });
    connect(d->project, &QComboBox::currentIndexChanged, this, [this] { d->onProjectChosen(); });
    connect(d->title, &QLineEdit::textChanged, this, [this] { d->updateCreate(); });
    connect(d->create, &QPushButton::clicked, this, [this] { d->accept(); });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);

    connect(&run, &QuickAddRun::proposed, this, [this](const QuickAddRun::Proposal &p) { d->onProposed(p); });
    connect(&run, &QuickAddRun::proposalsDone, this, [this] {
        d->reading = false;
        d->setStatus();
    });
    connect(&run, &QuickAddRun::found, this, [this](const std::vector<QuickAddItem> &found) { d->onFound(found); });
    connect(&run, &QuickAddRun::findingDone, this, [this] {
        d->finding = false;
        d->setStatus();
    });
    // Closing the dialog, by Cancel or otherwise, stops what still runs.
    connect(this, &QDialog::finished, this, [this] { d->run.cancel(); });

    resize(520, sizeHint().height());

    // A link or key on the clipboard is read at once.
    const QClipboard *clipboard = QGuiApplication::clipboard();
    const QString copied = clipboard ? clipboard->text().trimmed() : QString();
    if (!copied.isEmpty() && !copied.contains(QLatin1Char('\n')))
        setText(copied);
}

QuickAddDialog::~QuickAddDialog()
{
    // The run outlives the dialog, and must not answer into it.
    disconnect(&d->run, nullptr, this, nullptr);
    d->run.cancel();
}

void QuickAddDialog::setText(const QString &text)
{
    d->text->setText(text);
    d->read();
}

QString QuickAddDialog::createdTaskId() const { return d->created; }

QStringList QuickAddDialog::addedItemIds() const { return d->added; }

} // namespace mirante
