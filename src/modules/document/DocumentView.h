#pragma once

#include <QString>

#include <memory>

class QWidget;

namespace mirante {
class ItemContext;
}

namespace mirante::modules::document {

class DocumentSource;

// The view the three document types share: Reload and Open above, and the
// file in the format the item chooses, or a message. A failed reload keeps
// the file shown, with the reason above it. See docs/item-types.md#documents.
//
// openLocalLinks lets a relative link in the document open the linked
// local file, for project files.
QWidget *createDocumentView(ItemContext &context, std::unique_ptr<DocumentSource> source, bool openLocalLinks,
                            QWidget *parent);

// A message that the project folder has no local path on this device, with
// Choose folder…, as the Folder item shows.
QWidget *createChooseFolderView(ItemContext &context, const QString &folderId, QWidget *parent);

} // namespace mirante::modules::document
