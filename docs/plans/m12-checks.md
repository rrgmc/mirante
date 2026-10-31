# M12 checks by hand

These are the checks for [milestone M12](../roadmap.md#m12-document-items) that tests cannot make. Run them on Windows with the release build:

```powershell
task build PRESET=release
task run PRESET=release -- -Data <a test workspace> -Settings <a scratch .ini file>
```

You need a writable test workspace with a project that has a folder mapped on this device, a task to add items to, a Markdown file with an image in that folder, a small PDF, and a file over 50 MB.

Tick each box, or note what is wrong next to it.

**Result, 2026-10-31:** the user ran the checks with the release build and accepted M12. Every box is ticked.

## Adding items

- [x] Help > Modules lists "Documents" with its three types, its version, author and homepage.
- [x] The new item tree shows Document (web address), Document (workspace file) and Document (project file) under Documents, each with its own icon.
- [x] Pasting `https://raw.githubusercontent.com/rrgmc/mirante/refs/heads/master/README.md` into Add Item… offers Document (web address) before Web page, named `README.md`.
- [x] Pasting `https://github.com/rrgmc/mirante/blob/master/README.md` offers Document (web address) with the `raw.githubusercontent.com` address.
- [x] Document (workspace file)'s editor: **Choose file…** picks the PDF, shows its name, and OK adds the item. A file over 20 MB asks first; a file over 50 MB is refused with a message.
- [x] Document (project file)'s form has Folder, Path in the folder and Format.

## Dropping files

- [x] A PDF dragged from Explorer onto a task row adds a Document (workspace file), named after the file. Explorer keeps the file.
- [x] A Markdown file from the mapped project folder, dropped on the task's Items row, adds a Document (project file) with the folder and a relative path.
- [x] A `.zip` dropped on a task changes nothing, and the status bar says "No item type can show this file."
- [x] An image dropped on a task still opens the paste dialog and adds an Image item.

## Showing documents

- [x] The README from GitHub shows as Markdown, with its headings, tables and images.
- [x] Reload downloads it again without losing the scroll position. Open opens the address in the browser.
- [x] With the network off, Reload keeps the README and says "Could not reload: …" above it.
- [x] The PDF shows its pages one under another, fitted to the cell's width. `Ctrl` with the wheel zooms.
- [x] Editing the project's Markdown file in an editor and saving it updates the cell within a second, also from an editor that saves by renaming, such as VS Code.
- [x] A relative image in the project's Markdown file shows. A relative link to another file opens it in the external program.
- [x] A Format of Plain text on the Markdown item shows its source in a fixed-width font.
- [x] A project folder unmapped on this device shows "Folder not set on this device" with **Choose folder…**, which maps it.
- [x] The Notes preview still shows Markdown and loads no images.

## Packages

- [x] The Windows setup from `task test PRESET=release` with `-Package` installs a Mirante that shows a PDF.
