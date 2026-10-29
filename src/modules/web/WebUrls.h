// The address of a web page, read from an item's configuration, and the
// reading of a pasted address. Pure functions, so they are tested without a
// view.
#pragma once

#include <mirante/UrlMatch.h>

#include <QJsonObject>
#include <QUrl>

#include <optional>

namespace mirante::modules::web {

// The "url" key, if it is an http or https address with a host. Otherwise an
// empty URL.
QUrl pageUrl(const QJsonObject &config);

// Any http or https address with a host, as a Web page named after its host,
// for ItemType::fromUrl(). The match is generic, so the item dialog offers
// the types of other modules first. Null for any other address.
std::optional<UrlMatch> matchUrl(const QUrl &url);

} // namespace mirante::modules::web
