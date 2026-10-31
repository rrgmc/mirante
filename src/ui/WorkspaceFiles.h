#pragma once

#include <QString>
#include <QtGlobal>

#include <functional>

namespace mirante {

class Store;

// Copying a local file into the data folder's files/, for a document item's
// editor and for a file dropped on a task. Every file there syncs to every
// device, so large files ask first and very large ones are refused. See
// docs/decisions/0104-document-items.md. Implemented in WorkspaceFiles.cpp.
namespace workspacefiles {

// A file larger than this asks before it is copied.
constexpr qint64 kAskAbove = 20LL * 1024 * 1024;
// A file larger than this is refused.
constexpr qint64 kRefuseAbove = 50LL * 1024 * 1024;

// Copies the file at path into the store's files/, with the file's suffix,
// and returns the new file name. A file that is already in files/ is not
// copied again; its own name is returned. ask(question) says whether to copy a large
// file. tell(text) shows why the file was not copied. Returns an empty
// string if the file was not copied, after tell() for any reason but a "no"
// to ask().
QString copyIn(Store &store, const QString &path, const std::function<bool(const QString &question)> &ask,
               const std::function<void(const QString &text)> &tell);

} // namespace workspacefiles
} // namespace mirante
