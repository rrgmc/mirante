#pragma once

#include <QString>

namespace mirante {

// A folder's Git state, as `git status --porcelain=v2 --branch` reports it.
// ItemContext::gitStatus() gives it. See decision 0080.
struct GitState
{
    enum class Result {
        Ok,
        NotARepository, // the folder is not in a Git working tree
        GitMissing,     // no git program on the PATH
        Failed,         // git failed or took too long; see message
    };

    Result result = Result::Failed;
    QString branch;     // empty when the head is detached
    QString commit;     // the head's short hash; empty before the first commit
    bool hasUpstream = false;
    int ahead = 0;      // commits not on the upstream
    int behind = 0;     // upstream commits not here
    int staged = 0;     // entries with changes in the index
    int changed = 0;    // entries with changes in the working tree only
    int untracked = 0;
    int conflicts = 0;  // unmerged entries
    QString message;    // why, for Failed
};

} // namespace mirante
