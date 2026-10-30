# 0072. A pipeline item shows the pipelines started together

- Status: Accepted
- Date: 2026-10-30

## Context

Some repositories start several pipelines at once for one merge request, so the latest is not always the one that matters. A pipeline also moves faster than the poll interval of two minutes.

## Decision

- **A group.** For a merge request or a branch, the status also asks for its recent pipelines. Those created within 10 minutes of the latest form a group, all with their jobs.
- **Showing it.** The cell shows the latest of the group. A row of pills, one per pipeline in the group, switches between them. Previous and Next step through the older pipelines too.
- **Attention.** Any failed pipeline in the group counts, each with its own key, so a failure is not hidden by a later pipeline that passed.
- **Faster while running.** While the shown pipeline is unfinished and the cell is visible, the view asks for a refresh every 15 seconds. Otherwise the usual interval holds.
- **A new type,** GitLab CI Pipeline List, `gitlab.pipeline.list`, lists pipelines of a project: everyone's, mine, a branch's or a merge request's.

## Alternatives

- **Only Previous and Next.** Simple, but a failure in a sibling pipeline stays out of sight.
- **A setting for the refresh rate.** One more setting. A fixed rate, only while something is running, costs little.

## Consequences

- A pipeline item costs one more request per poll, and one per pipeline in the group for its jobs.
- The 10 minutes are a guess that the user's repositories will test.
