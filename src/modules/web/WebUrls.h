// The address of a web page, read from an item's configuration. A pure
// function, so it is tested without a view.
#pragma once

#include <QJsonObject>
#include <QUrl>

namespace mirante::modules::web {

// The "url" key, if it is an http or https address with a host. Otherwise an
// empty URL.
QUrl pageUrl(const QJsonObject &config);

} // namespace mirante::modules::web
