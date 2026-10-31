# 0115. CI builds without precompiled headers

- Status: Accepted
- Date: 2026-10-31

## Context

[Decision 0110](0110-ci-runs-once-per-change-and-caches-compiling.md) added ccache to CI, and [decision 0112](0112-precompiled-headers.md) added precompiled headers to all builds. In the CI runs after that, ccache served little in the jobs that use precompiled headers:

| Job | Precompiled headers | ccache hits | Compiling |
|---|---|---|---|
| `build (windows-2022)` | on | 7% | about 4.5 to 6 minutes |
| `build (ubuntu-24.04)` | on | 9 to 11% | about 3.5 to 5.5 minutes |
| `no-modules` (Linux) | off | 94 to 98% | about 1 minute or less |

The compiling times run from the end of configuring to the link of the app. The `no-modules` job compiles 328 files and the `build` jobs over 500, so its time alone does not show what the `build` jobs would take. The tests took only 21 to 34 seconds of the `build` jobs, so compiling is where a run spends its time. The Linux `build` cache was at or over its 500 MB limit, with 508 cleanups in one run, while the `no-modules` cache used about 3% of its limit. The cause is not known. Large precompiled header files could explain the Linux cleanups, but the Windows cache was at about 38% of its limit. There, 33 of 538 compile calls could not be cached, which may be the precompiled headers themselves. If each run makes them with different bytes, every file compiled with them would miss. Decision 0112 measured 30 seconds for a local build with every file in ccache, so the two can work together, but it did not measure a CI run with a filled cache. The first runs after this change show whether precompiled headers are what stops the hits.

## Decision

- **The `build` jobs in `ci.yml` compile with `MIRANTE_PCH=OFF`**, as `no-modules` already does.
- **Their ccache key changes to `<os>-ci-no-pch`,** so the old entries are not restored.
- **`tools/build.ps1` gets a `-NoPch` switch,** which the Windows job uses.
- **Local builds and `release.yml` keep precompiled headers.** A local build has ccache as well as precompiled headers, and it often compiles from nothing in a new worktree. A release does not use a ccache, so precompiled headers still shorten it.

This amends decision 0112 for CI only.

## Alternatives

- **A larger ccache limit.** This could stop the Linux cleanups. The Windows cache was at about 38% of its limit and still had 7% hits, so size is not the whole cause.
- **Keep precompiled headers and find why ccache misses.** MSVC and GCC would each need their own investigation, for a benefit only on builds that ccache cannot serve.
- **Choose what to build from the changed files.** This can follow, but with a working cache it saves less. It does not help a change that touches both the core and the modules.

## Consequences

- A build that ccache can serve should compile in a small part of the time it takes now. If a pull request run does not show hits near 90% once `master` has filled the cache, this decision should be reversed.
- A build that ccache cannot serve is slower: about 10.4 minutes of compiling on Windows instead of 5.9, by the numbers in decision 0112. This happens when a header used everywhere changes or the cache expires.
- The `build` jobs now find a file that misses an include, also in modules. The `no-modules` job finds this only outside the modules.
- `no-modules` still builds without precompiled headers, and it no longer needs to be the only job that does.
