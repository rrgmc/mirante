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
};

// The editor of an item's configuration, shown in the item dialog. An item
// type returns its own from ItemType::createEditor(); otherwise the core builds
// one from ItemType::fields(). See docs/decisions/0028-item-fields-in-m3.md.
class ItemEditor : public QWidget
{
public:
    using QWidget::QWidget;

    virtual void setConfig(const QJsonObject &config) = 0;
    // The edited configuration. Keys the editor does not show are kept as given
    // to setConfig().
    virtual QJsonObject config() const = 0;
    // What is wrong with the current values, one message each. Empty when valid.
    virtual QStringList problems() const { return {}; }
};

} // namespace mirante
