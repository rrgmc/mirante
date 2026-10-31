# 0113. CI skips pull requests that change only documentation

- Status: Accepted
- Date: 2026-10-31

## Context

Since [decision 0109](0109-pull-requests-into-master.md), every change reaches `master` through a pull request, and `master` requires the checks `build (windows-2022)`, `build (ubuntu-24.04)`, `no-modules` and `module-includes`. [Decision 0110](0110-ci-runs-once-per-change-and-caches-compiling.md) runs CI once per pull request. A pull request that changes only documents still built and tested the app on three platforms, which took about 8 minutes and could fail on a flaky test that the change did not touch.

GitHub's `paths-ignore` filter skips the whole workflow. The required checks then never report, and the pull request cannot be merged.

## Decision

- **A first job, `changes`, lists the pull request's files** with the GitHub API. If every file is under `docs/` or ends in `.md`, its output `code` is `false`. On a push to `master` it is always `true`, so the caches stay filled.
- **`no-modules` and `module-includes` skip as a whole** when `code` is `false`. A job skipped by its condition counts as passed for a required check.
- **The `build` matrix job skips each step instead of the job.** A skipped matrix job reports one check under its unexpanded name, `build (${{ matrix.os }})`, so the required checks with the platform in their names would never report. The job starts, skips its steps and passes in a few seconds.
- **The jobs skip only when `code` is exactly `false`.** If the `changes` job fails, the others still run, so a failure there can never let a change through unchecked.

No Markdown file is compiled into the app or read by a test, so skipping them is safe. If that changes, the rule must change with it.

## Alternatives

- **`paths-ignore` on the workflow.** The required checks would never report on a documentation pull request.
- **A separate workflow that reports the same check names for documentation pull requests.** Two workflows could then both report one check name, and a code change could pass on the documentation workflow's result.
- **`dorny/paths-filter`.** It does the same as the few lines in the `changes` job, but adds a third-party action.

## Consequences

- A pull request that changes only documents passes CI in about a minute.
- A pull request that mixes documents and code builds as before.
- A file that is not Markdown and not under `docs/`, such as `.claude/settings.json` or `CMakePresets.json`, still builds everything.
- The `changes` job adds a few seconds to every run, and each `build` step carries the condition.
