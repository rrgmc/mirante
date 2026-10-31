# 0110. CI runs once per change and caches compiling

- Status: Accepted
- Date: 2026-10-31

## Context

A CI run took 13 to 15 minutes. On Windows, compiling took about 10 minutes, the tests about 100 seconds and the NSIS setup about 100 seconds. Linux and macOS were similar, without the setup. Every run compiled all 958 build steps from nothing.

Since [decision 0109](0109-pull-requests-into-master.md), each work branch has a pull request. A push to such a branch started two runs of the same commit: one for the push and one for the pull request. A new push did not stop the runs for the commit before it.

CTest ran the 134 tests one at a time. Most of their time is spent waiting on timers, such as the two seconds before an item counts as seen.

## Decision

- **CI runs on pull requests and on pushes to `master`.** A push to a work branch starts no run of its own. Its pull request checks it.
- **A new push to a pull request cancels the run still going for it.** Runs on `master` are never cancelled.
- **Every build job compiles through ccache**, with `hendrikmuhs/ccache-action`. `CMakeLists.txt` already uses ccache when it finds it. Each job has its own cache of up to 500 MB. Only runs on `master` save it; pull requests read it.
- **Tests run in parallel.** CI sets `CTEST_PARALLEL_LEVEL` to 4. `tools/build.ps1` sets it to the number of cores, unless it is set already. Each test keeps its files in temporary folders of its own, so the tests do not disturb each other.

## Alternatives

- **Save the compile cache from pull requests too.** A pull request's cache is visible only to that pull request, and the repository has 10 GB of cache in all. The Qt caches use about 2.8 GB. Caches from pull requests would push `master`'s caches out.
- **Shorten the timers in the tests.** It would save a few seconds per test, but needs the product code to take its intervals from outside. With the tests in parallel, the slowest test sets the time, at about 10 to 15 seconds.
- **Skip the packages in pull requests.** It would save about 100 seconds on Windows and 90 on macOS, but a broken package would be found only on `master`.
- **sccache instead of ccache.** Local builds already use ccache, so CI and local builds work the same way.

## Consequences

- A pull request whose change touches a few files compiles only those files and what includes them, as long as `master` has run since the cache was emptied.
- The first run after a change to a widely included header, or to the compiler flags, still compiles everything.
- A push to a branch without a pull request is not built by CI.
- Running the tests in parallel can expose a test that depends on another one's files. Such a test must move its files to a temporary folder.
