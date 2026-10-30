#pragma once

#include <QString>

// Reads the branch checked out in a folder from its Git files, without
// starting a process. See docs/decisions/0080-git-read-by-the-core.md.
// Implemented in GitHead.cpp.

namespace mirante::git {

// The branch in <path>/.git/HEAD, such as "main" for "ref: refs/heads/main".
// If .git is a file, as in a worktree or a submodule, follows its "gitdir:"
// line, relative to path if it is relative. Empty when the head is detached,
// or the folder has no readable head.
QString branchAt(const QString &path);

// The HEAD file that branchAt() reads for path, to watch for changes, or empty
// if there is none.
QString headFile(const QString &path);

} // namespace mirante::git
