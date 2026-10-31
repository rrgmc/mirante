#include "LinksEditor.h"

#include "Links.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace mirante::modules::notes {

namespace {

QString textOfRow(const QJsonObject &link)
{
    const QString title = linkTitle(link);
    return title.isEmpty() ? QCoreApplication::translate("notes", "New link") : title;
}

} // namespace

LinksEditor::LinksEditor(const EditorContext &context, QWidget *parent)
    : ItemEditor(parent)
    , m_context(context)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *left = new QVBoxLayout;
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("linkList"));
    left->addWidget(m_list, 1);
    auto *buttons = new QHBoxLayout;
    auto *add = new QPushButton(QCoreApplication::translate("notes", "Add"), this);
    add->setObjectName(QStringLiteral("addLink"));
    m_remove = new QPushButton(QCoreApplication::translate("notes", "Remove"), this);
    m_remove->setObjectName(QStringLiteral("removeLink"));
    m_up = new QPushButton(QCoreApplication::translate("notes", "Up"), this);
    m_up->setObjectName(QStringLiteral("moveUp"));
    m_down = new QPushButton(QCoreApplication::translate("notes", "Down"), this);
    m_down->setObjectName(QStringLiteral("moveDown"));
    buttons->addWidget(add);
    buttons->addWidget(m_remove);
    buttons->addWidget(m_up);
    buttons->addWidget(m_down);
    left->addLayout(buttons);
    layout->addLayout(left, 1);

    m_fields = new QWidget(this);
    auto *form = new QFormLayout(m_fields);
    form->setContentsMargins(0, 0, 0, 0);
    m_title = new QLineEdit(m_fields);
    m_title->setObjectName(QStringLiteral("title"));
    form->addRow(QCoreApplication::translate("notes", "Title"), m_title);
    m_description = new QLineEdit(m_fields);
    m_description->setObjectName(QStringLiteral("description"));
    form->addRow(QCoreApplication::translate("notes", "Description"), m_description);
    m_kind = new QComboBox(m_fields);
    m_kind->setObjectName(QStringLiteral("kind"));
    m_kind->addItem(QCoreApplication::translate("notes", "Web address"), linkKindId(LinkKind::Url));
    m_kind->addItem(QCoreApplication::translate("notes", "Project file"), linkKindId(LinkKind::Project));
    m_kind->addItem(QCoreApplication::translate("notes", "Workspace file"), linkKindId(LinkKind::Workspace));
    form->addRow(QCoreApplication::translate("notes", "Leads to"), m_kind);

    // One page per kind, in the order of the kinds in m_kind.
    m_targets = new QStackedWidget(m_fields);

    auto *urlPage = new QWidget(m_targets);
    auto *urlForm = new QFormLayout(urlPage);
    urlForm->setContentsMargins(0, 0, 0, 0);
    m_url = new QLineEdit(urlPage);
    m_url->setObjectName(QStringLiteral("url"));
    m_url->setPlaceholderText(QStringLiteral("https://"));
    urlForm->addRow(QCoreApplication::translate("notes", "Address"), m_url);
    m_targets->addWidget(urlPage);

    auto *projectPage = new QWidget(m_targets);
    auto *projectForm = new QFormLayout(projectPage);
    projectForm->setContentsMargins(0, 0, 0, 0);
    m_folder = new QComboBox(projectPage);
    m_folder->setObjectName(QStringLiteral("folder"));
    for (const auto &[id, name] : m_context.folders)
        m_folder->addItem(name, id);
    projectForm->addRow(QCoreApplication::translate("notes", "Folder"), m_folder);
    auto *pathRow = new QHBoxLayout;
    m_path = new QLineEdit(projectPage);
    m_path->setObjectName(QStringLiteral("path"));
    m_path->setPlaceholderText(QCoreApplication::translate("notes", "The folder itself"));
    m_browse = new QPushButton(QCoreApplication::translate("notes", "Browse…"), projectPage);
    m_browse->setObjectName(QStringLiteral("browse"));
    pathRow->addWidget(m_path, 1);
    pathRow->addWidget(m_browse);
    projectForm->addRow(QCoreApplication::translate("notes", "Path"), pathRow);
    m_pathNote = new QLabel(projectPage);
    m_pathNote->setObjectName(QStringLiteral("pathNote"));
    m_pathNote->setWordWrap(true);
    m_pathNote->setTextFormat(Qt::PlainText);
    projectForm->addRow(m_pathNote);
    m_targets->addWidget(projectPage);

    auto *filePage = new QWidget(m_targets);
    auto *fileForm = new QFormLayout(filePage);
    fileForm->setContentsMargins(0, 0, 0, 0);
    auto *fileRow = new QHBoxLayout;
    m_file = new QLabel(filePage);
    m_file->setObjectName(QStringLiteral("fileName"));
    m_file->setTextFormat(Qt::PlainText);
    auto *choose = new QPushButton(QCoreApplication::translate("notes", "Choose file…"), filePage);
    choose->setObjectName(QStringLiteral("chooseFile"));
    choose->setEnabled(static_cast<bool>(m_context.saveFile));
    fileRow->addWidget(m_file, 1);
    fileRow->addWidget(choose);
    fileForm->addRow(QCoreApplication::translate("notes", "File"), fileRow);
    m_targets->addWidget(filePage);

    form->addRow(m_targets);
    layout->addWidget(m_fields, 2);

    m_projectChooser = [this](const QString &root) {
        return QFileDialog::getOpenFileName(this, QCoreApplication::translate("notes", "Choose a File"), root);
    };
    m_fileChooser = [this] {
        return QFileDialog::getOpenFileName(this, QCoreApplication::translate("notes", "Choose a File"));
    };

    connect(add, &QPushButton::clicked, this, [this] { addLink(); });
    connect(m_remove, &QPushButton::clicked, this, [this] { removeLink(); });
    connect(m_up, &QPushButton::clicked, this, [this] { moveLink(-1); });
    connect(m_down, &QPushButton::clicked, this, [this] { moveLink(1); });
    connect(m_browse, &QPushButton::clicked, this, [this] { browseProject(); });
    connect(choose, &QPushButton::clicked, this, [this] { chooseFile(); });
    connect(m_list, &QListWidget::currentRowChanged, this, [this](int row) {
        if (!m_loading)
            loadRow(row);
    });
    for (QLineEdit *edit : {m_title, m_description, m_url, m_path})
        connect(edit, &QLineEdit::textEdited, this, [this] { storeRow(); });
    connect(m_kind, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { storeRow(); });
    connect(m_folder, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] { storeRow(); });

    showLinks(-1);
}

void LinksEditor::setConfig(const QJsonObject &config)
{
    m_config = config;
    m_links = linksOf(config);
    showLinks(m_links.isEmpty() ? -1 : 0);
}

QJsonObject LinksEditor::config() const
{
    QJsonObject config = m_config;
    QJsonArray links;
    for (const QJsonObject &link : m_links)
        links.append(cleanedLink(link));
    if (links.isEmpty())
        config.remove(kLinks);
    else
        config.insert(kLinks, links);
    return config;
}

QStringList LinksEditor::problems() const
{
    QStringList problems;
    for (int i = 0; i < m_links.size(); ++i) {
        const QString problem = linkProblem(m_links.at(i));
        if (!problem.isEmpty())
            problems << QCoreApplication::translate("notes", "Link %1: %2").arg(i + 1).arg(problem);
    }
    return problems;
}

void LinksEditor::addLink()
{
    m_links << QJsonObject{{kLinkKind, linkKindId(LinkKind::Url)}};
    showLinks(m_links.size() - 1);
    m_title->setFocus();
}

void LinksEditor::removeLink()
{
    if (m_row < 0)
        return;
    m_links.removeAt(m_row);
    showLinks(std::min(m_row, static_cast<int>(m_links.size()) - 1));
}

void LinksEditor::moveLink(int by)
{
    const int target = m_row + by;
    if (m_row < 0 || target < 0 || target >= m_links.size())
        return;
    m_links.swapItemsAt(m_row, target);
    showLinks(target);
}

void LinksEditor::browseProject()
{
    const QString root = rootOfFolder();
    if (root.isEmpty() || !m_projectChooser)
        return;
    const QString chosen = m_projectChooser(root);
    if (chosen.isEmpty())
        return;
    const QString relative = QDir(root).relativeFilePath(chosen);
    if (!isInsideFolder(relative)) {
        m_pathNote->setText(QCoreApplication::translate("notes", "That file is not inside this folder."));
        return;
    }
    m_path->setText(relative);
    storeRow();
}

void LinksEditor::chooseFile()
{
    if (m_row < 0 || !m_context.saveFile || !m_fileChooser)
        return;
    const QString path = m_fileChooser();
    if (path.isEmpty())
        return;
    const QString name = m_context.saveFile(path);
    if (name.isEmpty())
        return;
    m_links[m_row].insert(kLinkFile, name);
    m_links[m_row].insert(kLinkOriginalName, QFileInfo(path).fileName());
    m_list->item(m_row)->setText(textOfRow(m_links.at(m_row)));
    showFile();
}

void LinksEditor::showLinks(int row)
{
    m_loading = true;
    m_list->clear();
    for (const QJsonObject &link : std::as_const(m_links))
        m_list->addItem(textOfRow(link));
    m_list->setCurrentRow(row);
    m_loading = false;
    loadRow(row);
}

void LinksEditor::loadRow(int row)
{
    m_row = row;
    m_loading = true;
    const QJsonObject link = row >= 0 ? m_links.at(row) : QJsonObject();
    m_fields->setEnabled(row >= 0);
    m_title->setText(link.value(kLinkTitle).toString());
    m_description->setText(link.value(kLinkDescription).toString());
    m_url->setText(link.value(kLinkUrl).toString());
    m_path->setText(link.value(kLinkPath).toString());

    // A folder that left the project stays chosen, under its id, until the
    // user chooses another.
    const QString folder = link.value(kLinkFolder).toString();
    int folderIndex = folder.isEmpty() ? 0 : m_folder->findData(folder);
    if (folderIndex < 0) {
        m_folder->addItem(folder, folder);
        folderIndex = m_folder->count() - 1;
    }
    m_folder->setCurrentIndex(folderIndex);

    const int kindIndex = m_kind->findData(linkKindId(linkKind(link)));
    m_kind->setCurrentIndex(kindIndex);
    m_targets->setCurrentIndex(kindIndex);
    m_loading = false;

    showFile();
    showFolderState();
    updateButtons();
}

void LinksEditor::storeRow()
{
    if (m_loading || m_row < 0)
        return;
    QJsonObject &link = m_links[m_row];
    link.insert(kLinkTitle, m_title->text());
    link.insert(kLinkDescription, m_description->text());
    link.insert(kLinkKind, m_kind->currentData().toString());
    // The keys of the other kinds stay until config(), so that trying another
    // kind loses nothing.
    link.insert(kLinkUrl, m_url->text());
    link.insert(kLinkPath, m_path->text());
    link.insert(kLinkFolder, m_folder->currentData().toString());
    m_targets->setCurrentIndex(m_kind->currentIndex());
    m_list->item(m_row)->setText(textOfRow(link));
    showFolderState();
}

void LinksEditor::showFile()
{
    const QJsonObject link = m_row >= 0 ? m_links.at(m_row) : QJsonObject();
    const QString original = link.value(kLinkOriginalName).toString();
    const QString file = link.value(kLinkFile).toString();
    if (file.isEmpty())
        m_file->setText(QCoreApplication::translate("notes", "No file chosen"));
    else
        m_file->setText(QCoreApplication::translate("notes", "%1 (a copy in the workspace)")
                            .arg(original.isEmpty() ? file : original));
}

void LinksEditor::showFolderState()
{
    const bool known = !rootOfFolder().isEmpty();
    m_browse->setEnabled(known);
    m_pathNote->setText(known || m_folder->count() == 0
                            ? QString()
                            : QCoreApplication::translate("notes", "Folder not set on this device"));
}

void LinksEditor::updateButtons()
{
    m_remove->setEnabled(m_row >= 0);
    m_up->setEnabled(m_row > 0);
    m_down->setEnabled(m_row >= 0 && m_row < m_links.size() - 1);
}

QString LinksEditor::rootOfFolder() const
{
    const QString folder = m_folder->currentData().toString();
    return folder.isEmpty() || !m_context.folderPath ? QString() : m_context.folderPath(folder);
}

} // namespace mirante::modules::notes
