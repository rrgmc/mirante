#pragma once

#include <QJsonValue>
#include <QString>
#include <QStringList>

namespace mirante {

// One field of an item type's configuration, or of a module's settings
// screen. The core builds a form from a list of fields and checks the values.
// See docs/decisions/0028-item-fields-in-m3.md and
// docs/decisions/0044-module-settings-in-the-workspace.md. Project settings
// use it too; see ModuleRegistrar::addProjectSetting().
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
        TextList,       // a JSON array of one-line texts, edited one per line
        // A token or password, edited hidden. Settings screens only. It is
        // never part of the stored values: the core keeps it on this device,
        // in the keychain under "<workspace id>/<profile id>/<module id>/<key>".
        // See decisions 0054 and 0059.
        Secret,
    };

    QString key;                // the key in Item::config, or in a settings screen's values
    QString label;              // shown before the editor, such as "Project path"
    Type type = Type::Text;
    QJsonValue defaultValue;    // used for a new item; Null for none
    bool required = false;
    QStringList choices;        // Choice: the stored values
    QStringList choiceLabels;   // Choice: the labels shown, in the same order
    QString placeholder;        // grey text in an empty editor
    // Text: values offered in a drop-down list. The user may still type
    // another. Such as the project paths of a module's settings.
    QStringList suggestions;
};

} // namespace mirante
