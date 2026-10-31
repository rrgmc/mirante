#include "DocumentEditor.h"

#include "DocumentFormat.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace mirante::modules::document {

namespace {

const QString kFile = QStringLiteral("file");
const QString kOriginalName = QStringLiteral("originalName");

} // namespace

DocumentEditor::DocumentEditor(const EditorContext &context, QWidget *parent)
    : ItemEditor(parent)
    , m_context(context)
{
    auto *form = new QFormLayout(this);
    form->setContentsMargins(0, 0, 0, 0);

    auto *fileRow = new QHBoxLayout;
    m_file = new QLabel(this);
    m_file->setObjectName(QStringLiteral("fileName"));
    m_file->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *choose = new QPushButton(QCoreApplication::translate("document", "Choose file…"), this);
    choose->setObjectName(QStringLiteral("chooseFile"));
    choose->setEnabled(static_cast<bool>(m_context.saveFile));
    fileRow->addWidget(m_file, 1);
    fileRow->addWidget(choose);
    form->addRow(QCoreApplication::translate("document", "File"), fileRow);

    const Field field = formatField();
    m_format = new QComboBox(this);
    m_format->setObjectName(QStringLiteral("format"));
    for (int i = 0; i < field.choices.size(); ++i)
        m_format->addItem(field.choiceLabels.at(i), field.choices.at(i));
    form->addRow(field.label, m_format);

    m_chooser = [this] {
        return QFileDialog::getOpenFileName(
            this, QCoreApplication::translate("document", "Choose a Document"), QString(),
            QCoreApplication::translate("document", "Documents (*.md *.markdown *.txt *.log *.pdf);;All files (*)"));
    };
    connect(choose, &QPushButton::clicked, this, [this] { chooseFile(); });
    showFile();
}

void DocumentEditor::setConfig(const QJsonObject &config)
{
    m_config = config;
    const int index = m_format->findData(config.value(kFormatKey).toString(kFormatAuto));
    m_format->setCurrentIndex(index < 0 ? 0 : index);
    showFile();
}

QJsonObject DocumentEditor::config() const
{
    QJsonObject config = m_config;
    config.insert(kFormatKey, m_format->currentData().toString());
    return config;
}

QStringList DocumentEditor::problems() const
{
    if (m_config.value(kFile).toString().isEmpty())
        return {QCoreApplication::translate("document", "Choose a file.")};
    return {};
}

void DocumentEditor::chooseFile()
{
    if (!m_context.saveFile || !m_chooser)
        return;
    const QString path = m_chooser();
    if (path.isEmpty())
        return;
    const QString name = m_context.saveFile(path);
    if (name.isEmpty())
        return;
    m_config.insert(kFile, name);
    m_config.insert(kOriginalName, QFileInfo(path).fileName());
    showFile();
}

void DocumentEditor::showFile()
{
    const QString original = m_config.value(kOriginalName).toString();
    const QString file = m_config.value(kFile).toString();
    if (file.isEmpty())
        m_file->setText(QCoreApplication::translate("document", "No file chosen"));
    else
        m_file->setText(original.isEmpty() ? file : original);
}

} // namespace mirante::modules::document
