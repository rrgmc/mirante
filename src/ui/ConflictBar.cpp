#include "ui/ConflictBar.h"

#include "core/Entities.h"
#include "core/Model.h"

#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QImage>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextBlock>
#include <QVBoxLayout>

namespace mirante {

namespace {

// The thing a conflict is about, as in "Replace task Fix login timeout with
// the other version?": an entity named from the model, the workspace file, an
// image, or else the file itself.
QString thingOf(const Store::Conflict &conflict, const Model &model)
{
    const QString &file = conflict.file;
    if (file == store::workspaceFileName())
        return ConflictBar::tr("the workspace file");

    const qsizetype slash = file.indexOf(QLatin1Char('/'));
    if (slash < 0)
        return file;
    const QString folder = file.left(slash);
    const QString name = file.mid(slash + 1);
    if (folder == QLatin1String("files"))
        return ConflictBar::tr("image %1").arg(name);

    const QString id = QFileInfo(name).completeBaseName();
    if (folder == QLatin1String("groups")) {
        if (const Group *group = model.group(id))
            return ConflictBar::tr("group %1").arg(group->name);
    } else if (folder == QLatin1String("projects")) {
        if (const Project *project = model.project(id))
            return ConflictBar::tr("project %1").arg(project->name);
    } else if (folder == QLatin1String("tasks")) {
        if (const Task *task = model.task(id))
            return ConflictBar::tr("task %1").arg(task->title);
    } else if (folder == QLatin1String("dashboards")) {
        if (const RootDashboard *dashboard = model.rootDashboard(id))
            return ConflictBar::tr("dashboard %1").arg(dashboard->name);
    }
    return file;
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

// A file's text for comparing: pretty-printed if it is JSON, as it is otherwise.
QString displayText(const QByteArray &content)
{
    const QJsonDocument json = QJsonDocument::fromJson(content);
    if (!json.isNull())
        return QString::fromUtf8(json.toJson(QJsonDocument::Indented));
    return QString::fromUtf8(content);
}

// Marks the lines of edit that differ from the line with the same index in
// otherLines.
void markDifferences(QPlainTextEdit *edit, const QStringList &otherLines)
{
    const QColor colour(255, 170, 0, 90);
    QList<QTextEdit::ExtraSelection> selections;
    for (QTextBlock block = edit->document()->firstBlock(); block.isValid(); block = block.next()) {
        const int index = block.blockNumber();
        if (index < otherLines.size() && otherLines.at(index) == block.text())
            continue;
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(colour);
        selection.format.setProperty(QTextFormat::FullWidthSelection, true);
        selection.cursor = QTextCursor(block);
        selections.append(selection);
    }
    edit->setExtraSelections(selections);
}

// One side of the compare dialog: the file's name above its content.
QWidget *buildSide(const QString &relPath, QWidget *content)
{
    auto *side = new QWidget;
    auto *label = new QLabel(relPath, side);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *layout = new QVBoxLayout(side);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(label);
    content->setParent(side);
    layout->addWidget(content, 1);
    return side;
}

QWidget *imageView(const QImage &image)
{
    auto *label = new QLabel;
    label->setAlignment(Qt::AlignCenter);
    label->setPixmap(QPixmap::fromImage(image));
    auto *scroll = new QScrollArea;
    scroll->setWidget(label);
    scroll->setWidgetResizable(true);
    return scroll;
}

QPlainTextEdit *textView(const QString &text)
{
    auto *edit = new QPlainTextEdit;
    edit->setReadOnly(true);
    edit->setLineWrapMode(QPlainTextEdit::NoWrap);
    edit->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    edit->setPlainText(text);
    return edit;
}

} // namespace

struct ConflictBar::Private
{
    ConflictBar *q = nullptr;
    Store *store = nullptr;
    const Model *model = nullptr;
    std::function<bool(const QString &)> confirm;

    QVBoxLayout *lines = nullptr;
    struct Line
    {
        Store::Conflict conflict;
        QWidget *widget = nullptr;
        QLabel *text = nullptr;
    };
    std::vector<Line> shown;

    // Rebuilds the lines from the store's conflicts. Old lines are deleted
    // later, as the click that led here may come from one of their buttons.
    void refresh()
    {
        for (const Line &line : shown) {
            lines->removeWidget(line.widget);
            line.widget->hide();
            line.widget->deleteLater();
        }
        shown.clear();

        for (const Store::Conflict &conflict : store->conflicts())
            shown.push_back(buildLine(conflict));
        q->setVisible(!shown.empty());
    }

    // Updates the lines' text after the model changed a name.
    void updateTexts()
    {
        for (const Line &line : shown)
            line.text->setText(describe(line.conflict, *model));
    }

    Line buildLine(const Store::Conflict &conflict)
    {
        auto *widget = new QWidget(q);
        widget->setObjectName(QStringLiteral("conflictLine"));

        auto *text = new QLabel(describe(conflict, *model), widget);
        text->setObjectName(QStringLiteral("conflictText"));
        text->setWordWrap(true);
        text->setTextInteractionFlags(Qt::TextSelectableByMouse);
        text->setToolTip(conflict.copy);

        auto *keep = new QPushButton(tr("Keep current"), widget);
        keep->setObjectName(QStringLiteral("keepCurrent"));
        keep->setToolTip(tr("Delete the other version"));
        QObject::connect(keep, &QPushButton::clicked, q, [this, conflict] { keepCurrent(conflict); });

        auto *useOther = new QPushButton(tr("Use other"), widget);
        useOther->setObjectName(QStringLiteral("useOther"));
        useOther->setToolTip(tr("Replace the current version with the other one"));
        QObject::connect(useOther, &QPushButton::clicked, q, [this, conflict] { useCopy(conflict); });

        auto *compare = new QPushButton(tr("Compare"), widget);
        compare->setObjectName(QStringLiteral("compare"));
        compare->setToolTip(tr("Show both versions side by side"));
        // Opened from the event loop, not from inside the click: a choice made
        // in the dialog rebuilds the lines, deleting this button.
        QObject::connect(compare, &QPushButton::clicked, q, [this, conflict] {
            QMetaObject::invokeMethod(q, [this, conflict] { showCompare(conflict); }, Qt::QueuedConnection);
        });

        auto *layout = new QHBoxLayout(widget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->addWidget(text, 1);
        layout->addWidget(keep);
        layout->addWidget(useOther);
        layout->addWidget(compare);
        lines->addWidget(widget);
        return {conflict, widget, text};
    }

    void keepCurrent(const Store::Conflict &conflict) { store->keepCurrent(conflict); }

    // Returns whether the copy was used.
    bool useCopy(const Store::Conflict &conflict)
    {
        const QString question = tr("Replace %1 with the other version? The current version is deleted.")
                                     .arg(thingOf(conflict, *model));
        if (!confirm(question))
            return false;
        store->useCopy(conflict);
        return true;
    }

    void showCompare(Store::Conflict conflict)
    {
        const QDir dir(store->dir());
        const QString filePath = dir.filePath(conflict.file);
        const QString copyPath = dir.filePath(conflict.copy);

        QDialog dialog(q->window());
        dialog.setObjectName(QStringLiteral("compareDialog"));
        dialog.setWindowTitle(describe(conflict, *model));
        dialog.resize(1000, 640);

        QWidget *current = nullptr;
        QWidget *other = nullptr;
        const QImage currentImage(filePath);
        const QImage otherImage(copyPath);
        if (conflict.file.startsWith(QLatin1String("files/")) && (!currentImage.isNull() || !otherImage.isNull())) {
            current = imageView(currentImage);
            other = imageView(otherImage);
        } else {
            const QString currentText = displayText(readFile(filePath));
            const QString otherText = displayText(readFile(copyPath));
            QPlainTextEdit *currentEdit = textView(currentText);
            QPlainTextEdit *otherEdit = textView(otherText);
            markDifferences(currentEdit, otherText.split(QLatin1Char('\n')));
            markDifferences(otherEdit, currentText.split(QLatin1Char('\n')));
            current = currentEdit;
            other = otherEdit;
        }
        current->setObjectName(QStringLiteral("currentVersion"));
        other->setObjectName(QStringLiteral("otherVersion"));

        auto *sides = new QHBoxLayout;
        sides->addWidget(buildSide(conflict.file, current), 1);
        sides->addWidget(buildSide(conflict.copy, other), 1);

        auto *keep = new QPushButton(tr("Keep current"), &dialog);
        keep->setObjectName(QStringLiteral("compareKeepCurrent"));
        keep->setAutoDefault(false);
        QObject::connect(keep, &QPushButton::clicked, &dialog, [this, conflict, &dialog] {
            keepCurrent(conflict);
            dialog.accept();
        });
        auto *useOther = new QPushButton(tr("Use other"), &dialog);
        useOther->setObjectName(QStringLiteral("compareUseOther"));
        useOther->setAutoDefault(false);
        QObject::connect(useOther, &QPushButton::clicked, &dialog, [this, conflict, &dialog] {
            if (useCopy(conflict))
                dialog.accept();
        });
        auto *close = new QPushButton(tr("Close"), &dialog);
        close->setObjectName(QStringLiteral("compareClose"));
        close->setDefault(true);
        QObject::connect(close, &QPushButton::clicked, &dialog, &QDialog::reject);

        auto *buttons = new QHBoxLayout;
        buttons->addStretch(1);
        buttons->addWidget(keep);
        buttons->addWidget(useOther);
        buttons->addWidget(close);

        auto *layout = new QVBoxLayout(&dialog);
        layout->addLayout(sides, 1);
        layout->addLayout(buttons);
        dialog.exec();
    }
};

ConflictBar::ConflictBar(Store &store, const Model &model, QWidget *parent)
    : QFrame(parent)
    , d(std::make_unique<Private>())
{
    d->q = this;
    d->store = &store;
    d->model = &model;
    d->confirm = [this](const QString &question) {
        return QMessageBox::question(window(), tr("Use other version"), question) == QMessageBox::Yes;
    };

    // Styled like the main window's warning bar.
    setObjectName(QStringLiteral("conflictBar"));
    setFrameShape(QFrame::StyledPanel);
    setAutoFillBackground(true);
    QPalette colours = palette();
    colours.setColor(QPalette::Window, colours.color(QPalette::ToolTipBase));
    colours.setColor(QPalette::WindowText, colours.color(QPalette::ToolTipText));
    setPalette(colours);

    d->lines = new QVBoxLayout(this);
    d->lines->setContentsMargins(8, 4, 4, 4);
    d->lines->setSpacing(4);

    connect(&store, &Store::conflictsChanged, this, [this] { d->refresh(); });
    const auto updateTexts = [this] { d->updateTexts(); };
    connect(&model, &Model::modelReset, this, updateTexts);
    connect(&model, &Model::groupAdded, this, updateTexts);
    connect(&model, &Model::groupChanged, this, updateTexts);
    connect(&model, &Model::projectAdded, this, updateTexts);
    connect(&model, &Model::projectChanged, this, updateTexts);
    connect(&model, &Model::taskAdded, this, updateTexts);
    connect(&model, &Model::taskChanged, this, updateTexts);
    connect(&model, &Model::rootDashboardAdded, this, updateTexts);
    connect(&model, &Model::rootDashboardChanged, this, updateTexts);
    d->refresh();
}

ConflictBar::~ConflictBar() = default;

QString ConflictBar::describe(const Store::Conflict &conflict, const Model &model)
{
    return tr("Two versions of %1").arg(thingOf(conflict, model));
}

void ConflictBar::setConfirm(std::function<bool(const QString &)> confirm) { d->confirm = std::move(confirm); }

} // namespace mirante
