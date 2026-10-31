# 0104. Document items show a file from a web address, the workspace or a project folder

- Status: Proposed
- Date: 2026-10-31

## Context

The user wants a cell that shows a document: a Markdown file such as a repository's `README.md` at a `raw.githubusercontent.com` address, a PDF kept with a task, or a design document in a project's repository.

No item type does this today:

- **Web page** shows a `raw.githubusercontent.com` address as plain text, because the server sends Markdown as `text/plain`. A `github.com/.../blob/...` address renders, but inside GitHub's whole page.
- **Notes** renders Markdown, but only its own text, kept in the task file.
- **Image** keeps a binary file in `files/` ([decision 0021](0021-image-items-as-files.md)), but shows only images.

Qt can render the formats natively. `QTextDocument::setMarkdown` reads CommonMark with GitHub's tables, task lists, strikethrough and autolinks. The Notes preview already uses it. `QPdfView`, from the Qt PDF module, shows PDF files in a widget.

The file can come from three places, and each behaves differently:

- **A web address.** It is downloaded, and downloaded again to see changes.
- **A file kept in the workspace.** It is copied into `files/`, so it syncs to the user's other devices, as Image files do.
- **A file in one of the project's folders.** Nothing is copied. Each device reads it at its own local path for the folder ([decision 0010](0010-folder-identity.md)), and the view follows changes made in an editor.

## Decision

- A new module, `document` ("Documents"), has three item types, one for each place a file comes from:
  - `document.url`, "Document (web address)";
  - `document.file`, "Document (workspace file)";
  - `document.project`, "Document (project file)".
- All three are native. They share one view, which takes the file's bytes and chooses a renderer. Only the way the bytes are read, and how changes are noticed, differs between the types.
- **Formats.** Markdown, plain text and PDF. Each item has a `format` field. Its default, Automatic, decides from the file's extension and then from the server's `Content-Type`; the extension comes first because servers often send Markdown as `text/plain`. Choosing a format by hand overrides both. Another format is added later as another renderer, not as another item type.
- **The module stays apart from Notes.** Notes and Documents share the Markdown display through a new view kit widget, `MarkdownView`, which the Notes preview also moves to. Modules cannot depend on each other, so the view kit is where shared widgets go.
- **Workspace files** are stored as `files/<uuid>.<ext>`, the same way as images. A file larger than 20 MB asks before it is copied, and a file larger than 50 MB is refused, because every file in the workspace syncs to every device. A large file can be shown from a project folder instead. The rules for deleting a file that no item refers to ([decision 0030](0030-deleting-image-files.md)) apply unchanged, since they already look for the file name in any item's configuration.
- **Project files** are referred to by a project folder and a path relative to it. They are never copied.
- **Pasted links.** `document.url` takes an address whose path ends in a known extension. It turns a `github.com/<owner>/<repo>/blob/<ref>/<path>` address into the matching `raw.githubusercontent.com` address. It is offered before Web page.
- **Dropped files.** A file of a known format, other than an image, dropped on a task in the sidebar becomes a new item of that task. Inside one of the project's folders it becomes a project file, and anywhere else a workspace file. Images still become Image items ([decision 0085](0085-drop-images-from-other-apps.md)).

The [item types](../item-types.md#documents) give the details.

## Alternatives

- **One item type with a `source` field.** The user prefers to pick the place in the new item tree, where the three types sit together under Documents ([decision 0100](0100-item-types-in-a-module-tree.md)). Each type's form also stays simple: one type would need its own editor only to show the fields of the chosen source.
- **The types in the Notes module, renamed "Notes and documents".** The Markdown code would be shared without the view kit. But every type id starts with its module's id, and the Notes type's id, `notes`, is stored in task files. Renaming the module's id would mean changing every task file, and a device on an older version would show its notes as an unknown type. Keeping the id `notes` instead gives ids such as `notes.url` that do not say what they are. And a build that wants Notes alone would have to take Qt PDF with it ([decision 0013](0013-compiled-in-modules.md)).
- **One item type per format,** such as "Markdown" and "PDF". The question the user answers when adding an item is "which file?", not "which format?". Downloading, reloading and watching are the same for every format, while each renderer is small.
- **Rendering in a web view,** with a JavaScript Markdown library. It would match GitHub more closely, with highlighted code and Mermaid diagrams, but each item would cost a web view's memory and be unloaded by the [memory policy](../web-views.md#memory-policy). Qt's renderer is good enough to read a document. A web rendering can be a separate type later, as [decision 0007](0007-separate-item-types-per-rendering.md) asks.
- **GitHub's Markdown API.** It renders exactly as GitHub does, but it needs the network for every view, and its rate limit is shared with the GitHub module.
- **Private repositories in `document.url`.** It would need tokens, and these belong to the GitHub and GitLab modules. A type in one of those modules can read a private file through its API later.

## Consequences

- The build needs the Qt PDF module (`Pdf` and `PdfWidgets`). It is not in the current Qt install or in the archives CI and the release workflows install. It must be added there, and to the packages, before the PDF renderer lands. Markdown and plain text do not need it.
- `EditorContext` can save only an image into `files/`. It needs a way to copy any file there, for the workspace file's editor and for drops.
- The sidebar accepts drops of files of the known formats, as well as images.
- The Notes preview moves to `MarkdownView`, with image loading off, so it behaves as before. A document turns image loading on: a relative image is read from next to the document, and an `http` or `https` image is downloaded.
- The workspace cannot hold a file larger than 50 MB. Such a file must sit in a project folder, which is not synced through the workspace.
