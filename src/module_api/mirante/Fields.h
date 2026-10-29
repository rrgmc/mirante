#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace mirante {

// One field of an item type's configuration. The core builds a form from a
// list of fields and checks the values. See
// docs/decisions/0028-item-fields-in-m3.md. Connection kinds and project
// settings use the same struct from M5.
struct Field
{
    enum class Type {
        Text,           // one line
        MultiLineText,
        Url,            // http or https
        Integer,
        Bool,
        Choice,         // one of choices
        Folder,         // the id of one of the project's folders
        Image,          // the name of a file in the data folder's files/
    };

    QString key;                // the key in Item::config
    QString label;              // shown before the editor, such as "Project path"
    Type type = Type::Text;
    QJsonValue defaultValue;    // used for a new item; Null for none
    bool required = false;
    QStringList choices;        // Choice: the stored values
    QStringList choiceLabels;   // Choice: the labels shown, in the same order
    QString placeholder;        // grey text in an empty editor
};

} // namespace mirante
