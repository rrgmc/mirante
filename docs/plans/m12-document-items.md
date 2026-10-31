# M12 code plan: document items

This plan builds [milestone M12](../roadmap.md#m12-document-items). After M12, a task can show a Markdown, plain text or PDF file from a web address, from the workspace's `files/` folder, or from one of the project's folders. See [decision 0104](../decisions/0104-document-items.md) and [item types](../item-types.md#documents).

## Decisions

- [Decision 0104](../decisions/0104-document-items.md), accepted: three item types in a new module `document`, one per place a file comes from, sharing one native view. Notes and documents share a `MarkdownView` in the view kit.

Smaller choices made here:

- **Size limits by format.** A Markdown or text file larger than 5 MB is not shown, and a PDF larger than 50 MB is not shown. `QTextDocument` and `QPlainTextEdit` lay out text on the main thread, and a 50 MB text file would hold up the window for seconds. This replaces the single 50 MB limit and the "rendered in the background" sentence in the item types.
- **Dropped files go through a new `ItemType::fromFile()`.** The core does not know the document types. Like `fromUrl()`, each type says whether it takes a dropped file and returns the configuration. A `FileMatch` can name a `copyKey`: the core copies the file into `files/` and stores the new name under that key, as `pastedImageField` does for images. The core applies the 20 MB and 50 MB workspace limits, since they are a property of the workspace, not of a module.
- **Which type takes a drop.** The core asks the types in the registry's order and takes the first match. The document types never both match: `document.project` takes a file inside one of the project's folders, `document.file` any other.
- **The sidebar takes any local file.** It cannot tell which types take a file without the task's folders, so it accepts a local file on a task row, and a file no type takes shows "No item type can show this file." in the status bar. Images still take the image path first.
- **Copying is shared.** `EditorContext::saveFile(path)` and the drop both call one core function, `WorkspaceFiles::copyIn()`, which asks above 20 MB, refuses above 50 MB and reports a failure, with the item dialog or the main window as the parent of its messages.
- **Watching.** A project file is watched with `QFileSystemWatcher` on the file and on its folder, so an editor that saves by renaming a new file over the old one is followed. Changes are gathered for 200 ms before the file is read again. A missing workspace file is looked for again every 10 seconds, because the core makes no event for a file that sync delivers to `files/`.
- **Images in Markdown.** `MarkdownView` answers `loadResource()` at once with a blank image and starts a fetch. When the image arrives, it is kept in a cache and the document is laid out again, keeping the scroll position. Downloads stop at 10 MB per image. A relative image of a project file is read only from inside the project folder.
- **Downloads.** A `QNetworkAccessManager` per view, with Qt's default redirect policy, which refuses a redirect from `https` to `http`. Downloads stop at 50 MB.
- **Text encoding.** UTF-8, or the encoding of a byte order mark found by `QStringConverter::encodingForData()`.

## Changes to the architecture

- `mirante/ItemType.h` gains `fromFile()`. A new header `mirante/FileMatch.h` has `FileDrop` and `FileMatch`.
- `EditorContext` gains `saveFile`.
- The view kit gains `MarkdownView`.
- `ui/WorkspaceFiles` is new: copying a file into `files/` with the size rules.
- `SidebarTree` emits `fileDropped(taskId, path)`, and `EntityActions::addFile()` makes the item.
- The top-level `find_package(Qt6 ...)` adds `Pdf` and `PdfWidgets`. `tools/setup-qt.ps1` and `tools/setup-qt.sh` install the `qtpdf` module, with `lib/cmake/Qt6PdfWidgets` as its marker. The CI and release cache keys change.

## Part 1: Qt PDF

- Add `qtpdf` to the `-m` list and the marker to both setup scripts.
- Add `-qtpdf` to the cache keys in `ci.yml` and `release.yml`.
- Add `Pdf PdfWidgets` to the top-level `find_package`.
- `tools/check-package.py` expects `Qt6Pdf` next to `Qt6Core` on Windows and Linux.

## Part 2: `MarkdownView`

- `mirante/widgets/MarkdownView.h`, `widgets/MarkdownView.cpp`: a read-only `QTextBrowser` with `setMarkdownText(text, base)`, `setResourceFetcher(fetcher)` and `setOpenLocalLinks(bool)`.
- Without a fetcher, `loadResource()` answers a blank image or a space, as the Notes preview does today. Web and mail links open in the external programs; a link to an anchor in the document scrolls to it; a relative link resolves against the base and opens externally only when local links are allowed.
- The Notes preview uses it, with no fetcher. Its tests stay unchanged.
- Tests in `src/module_api/widgets/tests`, or in the document module if the view kit has no test folder: no fetcher loads nothing; a fetcher receives resolved addresses and the image shows after it answers.

## Part 3: core

- `FileMatch.h`, `ItemType::fromFile()`, `EditorContext::saveFile`.
- `WorkspaceFiles::copyIn(path, parent, exec)`: reads the file, checks the limits, calls `Store::saveFile()` with the file's suffix, and returns the name, or empty after showing why.
- `SidebarTree`: a drag with a local file that is not an image is taken on task and Items rows, as images are. On the drop it emits `fileDropped`.
- `EntityActions::addFile(taskId, path)`: builds a `FileDrop` with the project's folders and their local paths, asks the types, copies the file when the match has a `copyKey`, and adds the item with the match's name and the fields' defaults. A read-only workspace, a file no type takes, and a failed copy each say so in the status bar.
- Tests: `EntityActions` adds a project file item and a workspace file item from a drop with a fake type; the size rules; the sidebar emits `fileDropped`.

## Part 4: the `document` module

| File | Contents |
|---|---|
| `DocumentModule.cpp` | The three types and the registration |
| `DocumentFormat.h/.cpp` | `Format`, the `format` field, and choosing a format from the setting, the file name and the content type |
| `DocumentLinks.h/.cpp` | `fromUrl()` for `document.url`, with the GitHub `blob` address |
| `DocumentSource.h/.cpp` | `DocumentSource`, with `UrlSource` (download, ETag, refresh timer) and `FileSource` (read, watch, look again while missing) |
| `DocumentView.h/.cpp` | The shared view: Reload and Open, the message or the content, and the reload error line |
| `DocumentRenderers.h/.cpp` | Markdown (`MarkdownView`), plain text and PDF |
| `DocumentEditor.h/.cpp` | The workspace file's editor: Choose file…, the file's name and the format |

Tests, on a local `QTcpServer` for downloads and a temporary folder for files:

- choosing the format, in every order of precedence;
- `fromUrl()`: extensions, the `blob` address, addresses it refuses;
- `fromFile()` of both file types;
- the view shows each format, shows the errors, keeps the content after a failed reload, follows a changed project file, and sends the ETag;
- the editor copies a file through `saveFile` and keeps unknown keys.

## Part 5: documents and translations

- The module's `README.md`, the module table in `docs/modules.md`, `fromFile()` under items from a link, the size limits in item types, and the roadmap.
- Brazilian Portuguese for every new text.

## Order of work

One branch, `m12-document-items`, in parts 1 to 5, one commit or more per part.

## Checking

`task check`, then by hand in a release build, with its own settings: a `raw.githubusercontent.com` Markdown address, a pasted `blob` address, a PDF chosen in the editor and one dropped on a task, a Markdown file in a project folder edited in an editor, and a file over 50 MB refused.

## Out of scope

- Private files on GitHub or GitLab.
- Highlighted code, Mermaid diagrams and other web-only Markdown features.
- Formats other than Markdown, plain text and PDF.
