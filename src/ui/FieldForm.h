#pragma once

#include <mirante/Fields.h>
#include <mirante/ItemEditor.h>

#include <functional>
#include <memory>
#include <vector>

class QDialog;

namespace mirante {

// An ItemEditor built from a list of fields: one row per field, with an editor
// that fits its type. Folder fields list EditorContext::folders. Image fields
// show a thumbnail, "Paste" and "Choose file…", and save through
// EditorContext::saveImage. "Paste" opens ImagePasteDialog first. An image
// dropped from another app on the field does the same as "Paste" (decision
// 0085). A text
// field with suggestions is an editable combo box. A text list is edited one
// entry per line. See the M3 plan, part 5, and decision 0044. Implemented in
// FieldForm.cpp.
class FieldForm : public ItemEditor
{
    Q_OBJECT

public:
    FieldForm(std::vector<Field> fields, EditorContext context, QWidget *parent = nullptr);
    ~FieldForm() override;

    void setConfig(const QJsonObject &config) override;
    QJsonObject config() const override;
    // The values typed into Secret fields, by key; only those changed and
    // not empty. config() never holds them. See decision 0054.
    QJsonObject secrets() const;

    // Whether a Secret field's token is set on this device. The placeholder
    // says so, and a set token gets a Remove button. Unknown until set. See
    // the M5 plan, part 8.
    enum class SecretState { Unknown, Set, NotSet };
    void setSecretState(const QString &key, SecretState state);
    // The keys of Secret fields whose Remove button was pressed.
    QStringList removedSecrets() const;
    // The messages of validate() for the current values.
    QStringList problems() const override;

    // The editor widget of a field, for tests. Null for an unknown key.
    QWidget *editorFor(const QString &key) const;
    // Runs the paste dialog that "Paste" and a drop open, and says whether it was
    // accepted. The default calls exec(); tests work the dialog instead.
    void setExec(std::function<bool(QDialog &dialog)> exec);

signals:
    void changed();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace mirante
