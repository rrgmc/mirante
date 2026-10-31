#pragma once

#include <QJsonObject>
#include <QString>

#include <utility>
#include <vector>

namespace mirante {

// A local file that the user dropped on a task, offered to each item type's
// ItemType::fromFile(). See docs/decisions/0104-document-items.md.
struct FileDrop
{
    // The absolute path of the dropped file.
    QString path;
    // The folders of the task's project that have a local path on this
    // device: the folder's id and its local path.
    std::vector<std::pair<QString, QString>> folders;
};

// What an item type makes of a dropped file, returned by ItemType::fromFile().
struct FileMatch
{
    // The configuration that shows the file, with the keys of the type's
    // fields. Keys left out take the fields' defaults.
    QJsonObject config;

    // A name for the new item, such as "README.md". Empty for none.
    QString name;

    // If set, the core copies the file into the data folder's files/ and
    // stores the copy's file name under this key. The copy follows the
    // workspace's size limits, and the item is not added if it fails.
    QString copyKey;
};

} // namespace mirante
