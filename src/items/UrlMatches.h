#pragma once

#include <mirante/UrlMatch.h>

#include <QString>
#include <QUrl>

#include <vector>

namespace mirante {

class ItemType;
class ItemTypeRegistry;

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
};

// The types of the registry that take url, best first: the types of modules
// before generic ones, such as Web page, and otherwise by name. profileId is
// the profile the new item would use; each type reads its servers there.
std::vector<TypeMatch> match(const ItemTypeRegistry &types, const QUrl &url, const QString &profileId);

} // namespace urlmatches
} // namespace mirante
