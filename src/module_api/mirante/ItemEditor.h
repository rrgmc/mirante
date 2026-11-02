#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>
#include <utility>
#include <vector>

class QImage;

namespace mirante {

// What a form or an editor can reach while an item is edited.
struct EditorContext
{
    // The profile the edited values belong to, or that the edited item or
    // project would use. Empty where none applies, such as device settings.
    // ItemEditor::setProfile() reports a later change.
    QString profileId;
    // The folders of the item's project: id and name.
    std::vector<std::pair<QString, QString>> folders;
    // Saves an image as PNG in the data folder's files/ and returns its file
    // name, or an empty string if it cannot.
    std::function<QString(const QImage &image)> saveImage;
    // Reads an image from files/ by its file name. A null image if it is missing.
    std::function<QImage(const QString &name)> loadImage;
    // Copies the file at path into files/ and returns its file name, or an
    // empty string if it was not copied. It asks before copying a file larger
    // than 20 MB and refuses one larger than 50 MB, and says why it did not
    // copy. See decision 0104.
    std::function<QString(const QString &path)> saveFile;
    // The local path of one of the project's folders on this device, or empty
    // if it has none.
    std::function<QString(const QString &folderId)> folderPath;
};

// The editor of an item's configuration or of a module's settings. A module
// returns its own from ItemType::createEditor(), SettingsScreen::createEditor(),
// or a factory it gives to ModuleRegistrar::setProjectEditor() or
// setDeviceEditor(). Otherwise the core builds a widgets::FieldForm from the
// fields. See decisions 0028 and 0120 in docs/decisions/.
class ItemEditor : public QWidget
{
    Q_OBJECT

public:
    using QWidget::QWidget;

    // Whether a token is set on this device. The token itself is never shown.
    enum class SecretState { Unknown, Set, NotSet };

    virtual void setConfig(const QJsonObject &config) = 0;
    // The edited configuration. Keys the editor does not show are kept as given
    // to setConfig().
    virtual QJsonObject config() const = 0;
    // What is wrong with the current values, one message each. Empty when valid.
    virtual QStringList problems() const { return {}; }

    // The profile changed while the editor is open, such as when the user
    // picks another profile in the item dialog. Does nothing by default.
    virtual void setProfile(const QString &profileId) { Q_UNUSED(profileId) }

    // Tokens, on a settings screen only. An editor can write a token but never
    // read it. Each key must be a Secret field in the screen's fields(), so the
    // core can load its state and delete it with its profile. See decision 0122.
    //
    // Whether the token of a field is set. Unknown until the core calls this.
    virtual void setSecretState(const QString &key, SecretState state)
    {
        Q_UNUSED(key)
        Q_UNUSED(state)
    }
    // The tokens the user entered, by field key: only those changed and not
    // empty. config() never holds them.
    virtual QJsonObject secrets() const { return {}; }
    // The keys of the tokens the user removed.
    virtual QStringList removedSecrets() const { return {}; }

signals:
    // The user changed a value. The settings window saves device settings on
    // it, so a device editor must emit it.
    void changed();
};

} // namespace mirante
