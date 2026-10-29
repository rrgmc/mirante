#pragma once

#include <mirante/UrlMatch.h>

#include <QString>
#include <QUrl>

#include <vector>

namespace mirante {

class ItemType;
class ItemTypeRegistry;
struct Connection;

// Asks the item types what they make of a pasted web address. The new item
// dialog and "Paste URL…" use it. See decision 0049. Implemented in
// UrlMatches.cpp.
namespace urlmatches {

// The text as a web address: http or https, with a host. Spaces around it
// are ignored. An empty URL for any other text.
QUrl webAddress(const QString &text);

// One type that takes the address, with what it made of it.
struct TypeMatch
{
    const ItemType *type = nullptr;
    UrlMatch match;
    // The connection of the group whose base URL is match.baseUrl, among the
    // type's connection kinds; empty if there is none.
    QString connectionId;
};

// The types of the registry that take url, best first: the types of modules
// before generic ones, such as Web page, and otherwise by name. connections
// are those of the new item's group; each type gets the base URLs of those of
// its connection kinds.
std::vector<TypeMatch> match(const ItemTypeRegistry &types, const QUrl &url,
                             const std::vector<Connection> &connections);

} // namespace urlmatches
} // namespace mirante
