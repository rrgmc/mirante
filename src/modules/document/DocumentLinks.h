#pragma once

#include <mirante/FileMatch.h>
#include <mirante/UrlMatch.h>

#include <QUrl>

#include <optional>

namespace mirante::modules::document {

// What Document (web address) makes of a pasted address: an http or https
// address whose path ends in a known extension. A GitHub blob address
// becomes its raw.githubusercontent.com address. Null otherwise. See
// docs/item-types.md#documents.
std::optional<UrlMatch> urlMatch(const QUrl &url);

// The raw.githubusercontent.com address of a github.com blob address, such
// as https://github.com/o/r/blob/main/README.md. url itself otherwise.
QUrl rawGitHubUrl(const QUrl &url);

// What Document (project file) makes of a dropped file: a file of a known
// format inside one of the project's folders. Null otherwise.
std::optional<FileMatch> projectFileMatch(const FileDrop &drop);

// What Document (workspace file) makes of a dropped file: a file of a known
// format outside the project's folders, copied into files/. Null otherwise.
std::optional<FileMatch> workspaceFileMatch(const FileDrop &drop);

// The path of file relative to folder, with "/" between its parts, if file
// is inside folder. Null otherwise.
std::optional<QString> pathInside(const QString &folder, const QString &file);

} // namespace mirante::modules::document
