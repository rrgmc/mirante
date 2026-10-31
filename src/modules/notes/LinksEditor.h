#pragma once

#include <mirante/ItemEditor.h>

#include <QJsonObject>
#include <QList>

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;

namespace mirante::modules::notes {

// The editor of a Links item: the list of links on the left, with buttons to
// add, remove and move them, and the selected link's fields on the right. See
// docs/item-types.md#links.
class LinksEditor : public ItemEditor
{
public:
    LinksEditor(const EditorContext &context, QWidget *parent);

    void setConfig(const QJsonObject &config) override;
    QJsonObject config() const override;
    QStringList problems() const override;

    // Ask for a file inside a folder, or for a file to copy into the
    // workspace. The defaults show a file dialog; tests set their own.
    void setProjectChooser(std::function<QString(const QString &root)> chooser)
    {
        m_projectChooser = std::move(chooser);
    }
    void setFileChooser(std::function<QString()> chooser) { m_fileChooser = std::move(chooser); }

private:
    void addLink();
    void removeLink();
    void moveLink(int by);
    void browseProject();
    void chooseFile();

    // Fills the list from m_links and selects a row, or none with -1.
    void showLinks(int row);
    // Fills the fields from a link.
    void loadRow(int row);
    // Writes the fields into the selected link.
    void storeRow();
    void showFile();
    void showFolderState();
    void updateButtons();
    QString rootOfFolder() const;

    EditorContext m_context;
    QJsonObject m_config;
    QList<QJsonObject> m_links;
    int m_row = -1;
    bool m_loading = false;

    QListWidget *m_list = nullptr;
    QPushButton *m_remove = nullptr;
    QPushButton *m_up = nullptr;
    QPushButton *m_down = nullptr;
    QWidget *m_fields = nullptr;
    QLineEdit *m_title = nullptr;
    QLineEdit *m_description = nullptr;
    QComboBox *m_kind = nullptr;
    QStackedWidget *m_targets = nullptr;
    QLineEdit *m_url = nullptr;
    QComboBox *m_folder = nullptr;
    QLineEdit *m_path = nullptr;
    QPushButton *m_browse = nullptr;
    QLabel *m_pathNote = nullptr;
    QLabel *m_file = nullptr;
    std::function<QString(const QString &root)> m_projectChooser;
    std::function<QString()> m_fileChooser;
};

} // namespace mirante::modules::notes
