---
name: mmp
description: Open a pull request for this worktree, merge it on a green CI, then remove the worktree and branch. Only when the owner types /mmp.
disable-model-invocation: true
model: sonnet
---

# Pull request, merge and clean

**`/mmp` is the owner asking for the merge.** It carries out the steps that `CLAUDE.md` and
[decision 0109](../../../docs/decisions/0109-pull-requests-into-master.md) set for bringing a branch
into `master`. It covers the work in the current worktree and nothing else.

**Each step finishes before the next one starts.** A step that fails stops the run, and the last
section says what to report.

## 1. Read where the session stands

```sh
git status --short
git branch --show-current
git log --oneline origin/master..HEAD
gh pr list --head <branch> --state open
```

- **The branch is `master`.** Stop. A session about to change tracked files puts itself in a
  worktree first, and `CLAUDE.md` says how.
- **The worktree holds files this session did not change.** Stop and ask, because they belong to
  somebody else's work.
- **There is nothing to commit and nothing ahead of `origin/master`.** Stop, because there is no
  pull request to open.
- **An open pull request already has this branch as its head.** Commit and push any new work to it
  (steps 2 and 3), then go to step 5.

**A session isolated in a worktree runs each git command on its own.** The harness refuses a git
command joined to another with `&&`, a pipe, a here-document or a `$(...)`. A commit message or a
pull request body therefore goes in a file in the scratchpad, and `git commit -F` or
`gh pr create --body-file` reads it.

## 2. Commit

The session's attribution lines end the message.

Stage the files by name. `git add -A` takes whatever else is in the worktree.

## 3. Check, then push

**A change that no compiler and no test reads leaves the build to CI.** That is prose under
`docs/`, a `README.md`, `CLAUDE.md` or a skill. A cold build in a fresh worktree takes a long time,
and CI builds it on Windows and Linux anyway.

**Anything else runs the local checks.** Source, a test, a translation, `CMakeLists.txt`, a preset,
a script, `Taskfile.yml` and a workflow all count as anything else.

```sh
task check-includes
task check-translations
task test PRESET=ci
task test PRESET=ci-no-modules
```

- **The `ci` presets treat warnings as errors**, as CI does. The `dev` preset of `task check` lets
  some MSVC warnings through that fail the Windows job.
- **`ci-no-modules` may be skipped** when the change touches nothing under `src/modules/` and no
  code that modules use. Say so in the pull request.

```sh
git push -u origin <branch>
```

## 4. Open the pull request

```sh
gh pr create --base master --title "<the commit's subject>" --body-file <file>
```

- **The body says what the change does and why**, in plain sentences, then which local checks ran
  and which did not. The session's pull request attribution ends it.
- **Several commits take a title of their own**, which states what the whole branch changes.

## 5. Wait for the required checks

**`master` requires four checks:** `build (windows-2022)`, `build (ubuntu-24.04)`, `no-modules` and
`module-includes`. `build (macos-15)` runs but is not required. The branch must also be up to date
with `master`.

**`gh pr checks --watch` can return before the checks exist.** It watches the checks GitHub has
listed at that moment, and right after a push there may be none. So poll the merge state instead,
in the background:

```sh
for i in $(seq 1 110); do
  state=$(gh pr view <n> --json mergeStateStatus --jq .mergeStateStatus)
  failed=$(gh pr checks <n> --required 2>&1 | grep -c fail)
  [ "$state" != BLOCKED ] || [ "$failed" != 0 ] && break
  sleep 30
done
gh pr checks <n>
```

- **`CLEAN`, or `UNSTABLE` with only `build (macos-15)` pending or failed**, goes on to step 6. A
  failed macOS build is still reported in step 8.
- **A failed required check stops the run.** Read the job's log (`gh run view <run> --log-failed`),
  and report the failure with its output. A fix is a new commit, a push and a new wait, when the
  fault is in this branch.
- **`DIRTY` or `BEHIND` stops the run.** The branch conflicts with `master` or is behind it. The fix
  is to merge `master` into the branch, resolve conflicts, check and push again, and the owner
  decides whether this run does that.
- **The Windows job takes about 15 minutes.** The loop above runs again when it ends before the
  checks do.

## 6. Merge

```sh
gh pr merge <n> --merge
gh pr view <n> --json state,headRefOid,mergeCommit
```

- **`--merge` makes a merge commit**, which is what `master`'s history holds. Give it the subject
  `Merge: <the pull request's title>` with `--subject`, to match the earlier merges.
- **No `--delete-branch`.** The repository deletes the remote branch itself. The flag reaches only
  the local branch, and from a worktree it fails after the merge has gone through, because it tries
  to check out `master`.
- **The state reads `MERGED` before anything is removed.**

## 7. Clean

**Prove the worktree holds nothing the merge lacks, and only then remove it.**

```sh
git status --short        # prints nothing
git rev-parse HEAD        # equals the pull request's headRefOid
```

`ExitWorktree` with `remove` cannot verify the worktree itself, and asks for `discard_changes`.
Both lines above are what make that flag safe. When either one fails, keep the worktree and say why.

Then, in the main checkout:

```sh
git pull --ff-only origin master
git branch -d <branch>    # only if ExitWorktree left the branch
git worktree list
```

- **`git branch -d`, never `-D`.** It refuses an unmerged branch, and that refusal is the check.
- **The pull comes first**, because `-d` compares against the local `master`.
- **Every other worktree stays.** Each belongs to another session's work.

## 8. Report

One short message, with these and nothing else:

- the pull request's link and the merge commit
- what was removed: the worktree, the local branch and the remote branch
- every check that did not run or did not pass, anywhere, including `build (macos-15)`

## When the run stops early

**Say which step stopped, what the command printed, and what state the run left behind.** A pull
request that is open and unmerged stays open. A worktree whose work is not merged stays where it is.
