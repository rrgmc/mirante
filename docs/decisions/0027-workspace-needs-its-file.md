# 0027. A workspace is a folder with `workspace.mirante`

- Status: Accepted
- Date: 2026-10-28

## Context

[Decision 0026](0026-workspace-file-name.md) kept `workspace.mirante` optional, so any folder could be opened as a workspace. Opening the wrong folder by mistake showed an empty workspace with no warning. The user asked that a data folder be defined by the `workspace.mirante` file in it.

## Decision

- A workspace is a folder with a `workspace.mirante` file at its top. The file is required. Its content is unchanged.
- File > Open Workspace… chooses the `workspace.mirante` file. A folder or the path of its `workspace.mirante` both open the same workspace.
- File > New Workspace… (`Ctrl+N`) chooses a folder and asks for a name, then writes `workspace.mirante` there and opens it. A folder that already is a workspace is just opened.
- A folder without the file is not opened. From the menus, for example Open Recent, the app offers to make it a workspace. Otherwise, as with `--data`, the warning bar says why.
- At start, the last workspace is reopened only if it still has its file; otherwise the app opens the sample data.
- Loading still reads the other files the same way. Writing `workspace.mirante` remains the only write in M2.

## Alternatives

- **Keep the file optional** (decision 0026). Any folder opens, so a wrong choice is not caught.
- **Create the file silently when a folder without one is opened.** It writes into folders the user may have chosen by mistake.

## Consequences

- A data folder is recognised by its file, so a wrong folder is caught before it is shown.
- Every workspace has a name from the start.
- A data folder made before this decision without `workspace.mirante` must be made a workspace once, through the offer or New Workspace….
