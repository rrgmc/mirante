#pragma once

#include <mirante/ItemEditor.h>

#include <QJsonObject>

#include <functional>

class QComboBox;
class QLabel;

namespace mirante::modules::document {

// The editor of Document (workspace file): a notice that the file is copied
// into the workspace, Choose file…, which copies a file into files/ through
// EditorContext::saveFile, and a choice of format. See
// docs/item-types.md#documents.
class DocumentEditor : public ItemEditor
{
public:
    DocumentEditor(const EditorContext &context, QWidget *parent);

    void setConfig(const QJsonObject &config) override;
    QJsonObject config() const override;
    QStringList problems() const override;

    // Asks for the file to copy. The default shows a file dialog; tests set
    // their own.
    void setChooser(std::function<QString()> chooser) { m_chooser = std::move(chooser); }
    // Runs Choose file….
    void chooseFile();

private:
    void showFile();

    EditorContext m_context;
    QJsonObject m_config;
    QLabel *m_file = nullptr;
    QComboBox *m_format = nullptr;
    std::function<QString()> m_chooser;
};

} // namespace mirante::modules::document
