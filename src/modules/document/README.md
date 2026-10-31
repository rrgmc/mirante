# document module

A Markdown, plain text or PDF file, from a web address, from the workspace or from one of the project's folders. See [item types](../../../docs/item-types.md#documents) and [decision 0104](../../../docs/decisions/0104-document-items.md).

## Item types

| Id | Name | Rendering | Description |
|---|---|---|---|
| `document.url` | Document (web address) | native | A Markdown, text or PDF file at a public web address, such as a README on GitHub, downloaded again on Reload or on a timer. |
| `document.file` | Document (workspace file) | native | A Markdown, text or PDF file kept in the workspace, so it syncs to your other devices. |
| `document.project` | Document (project file) | native | A Markdown, text or PDF file in one of the project's folders, shown again when it changes on disk. |

All three have the short name "Doc". The module calls itself "Documents" and describes itself as: "Markdown, text and PDF files, from a web address, the workspace or a project folder." Help > Modules shows the names and descriptions. See [decision 0056](../../../docs/decisions/0056-modules-window.md).

Its UUID is `21798809-decf-4f90-9029-795457d83198`. It is at version 0.1.0, by Rangel Reale. See [decision 0089](../../../docs/decisions/0089-module-identity.md).

The module links Qt Network, Qt PDF and Qt PDF Widgets with `QT_LIBS`.

## Files

| File | Contents |
|---|---|
| `DocumentModule.cpp` | The three types and the registration |
| `DocumentFormat.h/.cpp` | The formats, the `format` field, choosing a format, the size limits and decoding text |
| `DocumentLinks.h/.cpp` | Pasted addresses, GitHub `blob` addresses, and dropped files |
| `DocumentSource.h/.cpp` | `UrlSource` and `FileSource`, which read the file and say when it changes, and `ResourceFetcher`, which reads Markdown images |
| `DocumentView.h/.cpp` | The shared view and its renderers |
| `DocumentEditor.h/.cpp` | The workspace file's editor |

## View

`createDocumentView()` makes the view the three types share, from a `DocumentSource`.

- **The bar:** **Reload** reads the file again. **Open** opens it in the external program, from the source's location: the web address, or the local file. A failed reload of a file already shown keeps it, and the bar says "Could not reload: <reason>."
- **Format:** `chooseFormat()` takes the `format` setting, else the file name's extension, else the server's `Content-Type`. An unknown format says "This file's format is not known. Choose one in the item's settings."
- **Size:** Markdown and text larger than 5 MB, and PDF larger than 50 MB, say "This file is too large to show." No file larger than 50 MB is read or downloaded.
- **Markdown:** the view kit's `MarkdownView`, with a fetcher. Relative images resolve against the source's location. `ResourceFetcher` downloads `http` and `https` images and reads local ones only inside the project folder, each up to 10 MB. Relative links open externally only for project files.
- **Plain text:** a read-only `QPlainTextEdit` in the fixed-width font, wrapped. Text is UTF-8, or the encoding of a byte order mark.
- **PDF:** a `QPdfView`, one page under another, fitted to the width. `Ctrl` with the wheel zooms. A file that is not a PDF says "The PDF file cannot be read."
- Each renderer is made once and kept, so reloading the same format keeps the scroll position.

### Sources

- **`UrlSource`** (`document.url`): a `GET` without a login, with Qt's redirect policy that refuses `https` to `http`, and a 30-second transfer timeout. Later downloads send `If-None-Match` and `If-Modified-Since`, and a `304` keeps the file shown. `refresh` minutes above 0 download again on a timer.
- **`FileSource`** (`document.file`, `document.project`): reads the file and watches it and its folder with `QFileSystemWatcher`. Changes are gathered for 200 ms, and a change of another file in the folder is ignored. A workspace file that is missing is looked for again every 10 seconds.

### Messages

| Case | Message |
|---|---|
| `document.url` without an `http` or `https` address | "Set the address in the item's settings." |
| `document.file` without a file | "Choose the file in the item's settings." |
| A workspace file missing on this device | "The file is missing. It may not have synced to this device yet." |
| `document.project` without a path, or with an absolute path | "Set the path in the item's settings.", "The path must be relative to the folder." |
| A project folder with no local path on this device | "Folder not set on this device", with **Choose folder…** |
| A missing project file | "The file is missing." It is shown when it appears. |
| A failed download | "The file could not be downloaded: <reason>" |

## Configuration

| Type | Key | Field type | Description |
|---|---|---|---|
| all | `format` | choice | `auto` (the default), `markdown`, `text` or `pdf` |
| `document.url` | `url` | URL, required | The file's address |
| `document.url` | `refresh` | integer | Minutes between downloads; 0, the default, for never |
| `document.file` | `file` | | The copy's file name in the data folder's `files/`, set by the editor |
| `document.file` | `originalName` | | The file's name when it was chosen, which chooses the format |
| `document.project` | `folder` | folder, required | One of the project's folders |
| `document.project` | `path` | text, required | The file's path in the folder, with `/` between its parts |

`document.file` has its own editor, `DocumentEditor`: **Choose file…** copies a file through `EditorContext::saveFile`, which asks above 20 MB and refuses above 50 MB, and a choice of format. Its `fields()` holds only `format`, for the defaults of a dropped file.

## Links and drops

- `document.url` takes a pasted `http` or `https` address whose path ends in `.md`, `.markdown`, `.txt`, `.log` or `.pdf`. A `github.com/<owner>/<repo>/blob/<ref>/<path>` address becomes `https://raw.githubusercontent.com/<owner>/<repo>/<ref>/<path>`. The name is the file's name.
- `document.project` takes a dropped file of a known format inside one of the project's folders. `document.file` takes one anywhere else, with `copyKey` `file`, so the core copies it into `files/`.

## Settings

None.
