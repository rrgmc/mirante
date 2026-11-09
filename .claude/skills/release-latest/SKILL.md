---
name: release-latest
description: Release the core, mirante-modules and mirante-modules-work from their master at one version, skipping any with no changes, then release the Mirante app pinned to their latest tags. Only when the owner types /release-latest <X.Y.Z>.
argument-hint: <X.Y.Z>
disable-model-invocation: true
---

# Release the latest core and modules, then the app

**`/release-latest <X.Y.Z>` is the owner asking for this release.** It releases every library
repository that has changes on its `master` at version `X.Y.Z`, then releases Mirante built from
the libraries' latest tags. The app always takes `X.Y.Z`. A library with no commits since its last
tag is not released: it keeps its version, and the app stays pinned to its last tag. See decision
0134 in `mirante-core` for the release tags.

Below, **the released libraries** are the libraries this run releases, and **a library's tag** is
`vX.Y.Z` for a released library and its last tag for the others.

The version is `$ARGUMENTS`. Below it is written `X.Y.Z`, and the tag `vX.Y.Z`.

**The invocation counts as the owner's request to merge** the release pull requests, as
`/mmp` does. It does not count as a request to push the app's tag: step 7 asks first, because that
tag publishes a signed build to the public.

**Each step finishes before the next one starts.** A step that fails stops the run, and the last
section says what to report.

## Where the repositories are

The repositories are checkouts side by side in one parent folder, which is not a repository
itself. `<root>` below is that folder: the parent of this repository's main checkout, such as
`F:\prog\personal\mirante`. The skill runs the same from `<root>` or from inside `<root>/mirante`.

| Repository | Folder | Version file | Release on merge |
|---|---|---|---|
| `rrgmc/mirante-core` | `<root>/mirante-core` | `VERSION` | `release-tag.yml` tags and publishes |
| `rrgmc/mirante-modules` | `<root>/mirante-modules` | `VERSION` | `release-tag.yml` tags and publishes |
| `rrgmc/mirante-modules-work` | `<root>/mirante-modules-work` | `VERSION` | `release-tag.yml` tags and publishes |
| `rrgmc/mirante` | `<root>/mirante` | `MIRANTE_VERSION` in `CMakeLists.txt` | a pushed tag runs `release.yml` |

**Run every git command with `git -C <folder>`**, and never `cd` between repositories. Each
repository's main checkout stays on a clean `master`: all edits happen in a worktree at
`<folder>/.claude/worktrees/release-X.Y.Z`, on the branch `release-X.Y.Z`.

**GitHub work goes through the GitHub MCP tools**, not the `gh` CLI: opening, reading and merging
pull requests, reading check runs and releases. Plain `git` commits, pushes and tags. The remotes
use SSH, so a push that changes a workflow works.

**Commit messages and pull request bodies go in files in the scratchpad.** Pass them with
`git commit -F <file>`. Never put Markdown with backticks inline in a PowerShell string.

## 1. Check the version and the repositories

- **`X.Y.Z` must match `^[0-9]+\.[0-9]+\.[0-9]+$`.** Otherwise stop and ask for a version.
- **Fetch each repository**: `git -C <folder> fetch --tags origin`.
- **`vX.Y.Z` must not exist** in any of the four: `git -C <folder> ls-remote --tags origin refs/tags/vX.Y.Z`
  prints nothing. A library that already has the tag is the one exception: a run that stopped
  after that library's release resumes, and the library is skipped.
- **`X.Y.Z` must be higher** than each library's `VERSION` on `origin/master` and than the app's
  `MIRANTE_VERSION`. Compare as numbers, not as text.
- **No worktree or branch `release-X.Y.Z` exists yet** in a repository whose release is still to
  make. If one does, an earlier run left it: stop and ask.

For each library, list what `master` holds since its last tag:

```sh
git -C <folder> describe --tags --abbrev=0 origin/master
git -C <folder> log --oneline --first-parent <last tag>..origin/master
```

**A library with no commits since its last tag is not released.** Do not ask: skip it in steps 2
and 3, and leave its pins in the app at its last tag. When no library has commits, only the app
is released.

Show the owner a short plan: each library's current version, its last tag, the number of
commits it gains, and which libraries are skipped. Then go on without waiting.

## 2. Prepare each library's release

Do this for each released library, in the order core, `mirante-modules`, `mirante-modules-work`.

```sh
git -C <folder> worktree add -b release-X.Y.Z <folder>/.claude/worktrees/release-X.Y.Z origin/master
```

In the worktree:

1. **Run the changelog script** from the core's main checkout, which holds the newest
   `master`:

   ```sh
   python3 <root>/mirante-core/tools/release-changelog.py --root <worktree> release X.Y.Z
   ```

   It writes `X.Y.Z` to `VERSION`. It makes the section `## [X.Y.Z] - <today>` of `CHANGELOG.md`
   from the entries under Unreleased and the fragments in `changelog.d/`, moves the link references
   at the end, and deletes the fragments. In the core it also moves the fragments of
   `src/module_api/changelog.d/` into the interface changelog. See decision 0154 in the core. A
   failure, such as a fragment that is not well formed, stops the run.
2. **Read the new section and finish it by hand:**
   - **The module repositories open the section with one sentence** that sums up the release, and
     says which core it needs, such as "It needs the core's vX.Y.Z." Put it right under the
     section's heading. The core has no such sentence. When the core is not released, name the
     core's last tag instead.
   - **When Unreleased and `changelog.d/` were both empty but commits exist**, the script stops
     with "Nothing to release". Write the entries as fragments in `changelog.d/` from the merged
     pull requests since the last tag (`git log --first-parent`), in the changelog's own style:
     Keep a Changelog headings, one sentence per change, written for the people who use the
     repository. Read the pull requests through the MCP tools when a title is not enough. Then run
     the script again.
3. **Commit** with the subject `Release X.Y.Z`, a body that names the changelog's highlights in a
   sentence or two, and the session's attribution lines.
4. **Push**: `git -C <worktree> push -u origin release-X.Y.Z`.
5. **Open the pull request** with the MCP tool, base `master`, head `release-X.Y.Z`, title
   `Release X.Y.Z`. The body says which version this releases, that merging it tags `vX.Y.Z`
   and publishes its GitHub release, and ends with the session's pull request attribution.

Open every pull request before waiting on any of them. Their checks then run at the same time.

## 3. Merge each library's release

For each library pull request:

1. **Wait for the checks.** MCP calls cannot sleep, so run `sleep 300` with `run_in_background`,
   and when it ends read the pull request's status and check runs with the MCP tool. Repeat until
   every check is done. The Windows build takes about 15 minutes.
   - **All required checks pass** (the `Release tag` check among them, which proves the changelog
     has the section): merge.
   - **A required check fails**: stop the run. Report the check and its output. The other pull
     requests stay open.
   - **The branch is behind `master`**: stop and ask whether to merge `master` into it.
2. **Merge** with the MCP merge tool, merge method `merge`, commit title
   `Merge pull request #<n> from rrgmc/release-X.Y.Z` as GitHub gives it. Do not delete the branch
   from the tool: the repository deletes it.
3. **Wait for the tag.** The push to `master` runs `release-tag.yml`, which makes `vX.Y.Z` and its
   release within a minute or two. Write this to `wait-tag.sh` in the scratchpad and run
   `sh <scratchpad>/wait-tag.sh <folder> X.Y.Z` in the background:

   ```sh
   #!/bin/sh
   for i in $(seq 1 40); do
     tag=$(git -C "$1" ls-remote --tags origin "refs/tags/v$2")
     if [ -n "$tag" ]; then echo "$tag"; exit 0; fi
     sleep 15
   done
   echo "no tag v$2 after 10 minutes"
   exit 1
   ```

   Then read the release `vX.Y.Z` with the MCP release tool. **A missing tag or release stops the
   run.** The MCP tools cannot read Actions runs, so read the run on the repository's public
   Actions page in the built-in browser to say why.
4. **Clean up**, in this order:

   ```sh
   git -C <worktree> status --short          # prints nothing
   git -C <folder> worktree remove <worktree>
   git -C <folder> pull --ff-only origin master
   git -C <folder> branch -d release-X.Y.Z   # -d, never -D
   ```

## 4. Prepare the app's release

```sh
git -C <root>/mirante fetch origin
git -C <root>/mirante worktree add -b release-X.Y.Z <root>/mirante/.claude/worktrees/release-X.Y.Z origin/master
```

In the worktree, change these and nothing else:

1. **Each released library's pins move to `vX.Y.Z`.** A skipped library's pins stay as they are.
   The core's ref is in four places and CI fails when they differ:
   - `mirante.json`: the core's `ref`, and the `ref` of each module repository;
   - `.github/workflows/ci.yml`, `release.yml` (two lines) and `update.yml`: the
     `rrgmc/mirante-core/.github/workflows/...@<ref>` lines.
2. **New module folders join the manifest.** A folder with a module in a released module
   repository at `vX.Y.Z` that `mirante.json` does not list goes into that repository's `folders`,
   in alphabetical order. A folder that holds a module has a `CMakeLists.txt` and a `README.md`;
   compare with `git -C <folder> ls-tree -d --name-only vX.Y.Z`. A folder the app leaves out on
   purpose is not added: when in doubt, ask.
3. **`MIRANTE_VERSION`** in `CMakeLists.txt` becomes `X.Y.Z`.
4. **`CHANGELOG.md`** gets its section from the script, as in step 2, which also moves this
   repository's own fragments in `changelog.d/` into it:

   ```sh
   python3 <root>/mirante-core/tools/release-changelog.py --root <worktree> release X.Y.Z
   ```

   The app has no `VERSION` file, so the script leaves `MIRANTE_VERSION` to item 3. When
   Unreleased and `changelog.d/` are both empty, it stops with "Nothing to release": then first
   write a fragment in `changelog.d/` with the entries below, and run it again. Finish the section
   by hand: it opens with "Built from the core's <tag>, `mirante-modules` <tag> and
   `mirante-modules-work` <tag>.", with each library's tag, and holds the user-facing changes from
   the released libraries' `X.Y.Z` sections, merged under Added, Changed, Removed and Fixed, and
   written for people who use Mirante, not for developers. Leave out changes that only touch CI,
   builds, tests, the module interface or the docs.
5. **`README.md`** lists each module and its item types. A new module or item type in the
   libraries' changelogs gets its row or its name in the table, in the README's style. Read the
   README's section in `CLAUDE.md` before changing anything else in it.

**Build locally before pushing**, because the change touches `mirante.json` and workflows. The core's
script fetches the pinned core and modules:

```sh
<root>/mirante-core/tools/build.ps1 -Source <worktree> -Preset ci
```

Run it in the background: it takes several minutes. A failed build stops the run.

Commit with the subject `Prepare X.Y.Z with the latest core and modules` and a body that names the
pins, any new module folders and the README's new item types. Push, and open the pull request with
the MCP tool, with the same title and a body that lists the library releases, the skipped
libraries and the local build's result.

## 5. Merge the app's release

**`master` requires two checks:** `build / build (windows-2022)` and `build / build (ubuntu-24.04)`.
`build / build (macos-15)` and `build / sign-macos` run but are not required. Wait as in step 3.

- **Both required checks pass**: merge with merge method `merge`, even when a macOS job is still
  running or failed. Step 8 reports a failed macOS job.
- **A required check fails**: stop the run and report it.

Then clean up the worktree and branch as in step 3, and note the merge commit's hash.

## 6. Check master

The pull request's checks ran on the branch, not on the merge commit. Read the `CI` run on
`master` for the merge commit, through the MCP check runs of that commit or the public Actions page.
Wait for it as in step 3. A failure here stops before the tag, because the tag would build the same
commit.

## 7. Tag the app

**Ask the owner before pushing the tag.** Say which commit it tags, that the library releases are
out, and the state of the macOS jobs. The owner may want to test the macOS build first. Push only
on a clear yes.

```sh
git -C <root>/mirante tag -a vX.Y.Z -m "Release vX.Y.Z" <merge commit>
git -C <root>/mirante push origin refs/tags/vX.Y.Z
```

The tag runs `release.yml`, which builds on three systems, signs and notarises the macOS disk
image, and publishes the release with the changelog's section as notes. It takes about 30 minutes.
Wait with `sleep 600` in the background, then read the release `vX.Y.Z` with the MCP release tool,
until it exists with its three files or the run fails. The run itself is on the public Actions page.

## 8. Report

One short message, with these and nothing else:

- each library's pull request, merge commit, tag and release link
- the app's pull request, merge commit, tag and release link, or that the tag waits for the owner
- the pins, new module folders and README rows the app gained
- every check that did not run or did not pass, anywhere, including the macOS jobs

## When the run stops early

**Say which step stopped, what the command or tool returned, and what state the run left behind**:
which releases are out, which pull requests are open, which worktrees remain. A run started again
with the same version skips the libraries that already have `vX.Y.Z`, so leave released libraries
as they are. An open pull request stays open, and a worktree whose work is not merged stays where it is.
