# 0007. Separate item types for summary and page

- Status: Accepted
- Date: 2026-10-28

## Context

A merge request, a GitLab issue or a Jira issue can be shown as a native summary built from API data, or as the service's web page.

## Decision

Make them separate item types, for example `gitlab.mr.summary` and `gitlab.mr.page`. A task that needs both adds both items.

## Alternatives

- **One type with a switch between summary and page,** stored per placement.
- **A fixed rendering per type.**

## Consequences

- Each type stays simple: native or web, never both.
- Both views of the same merge request can be on screen side by side.
- Adding a merge request from a link can offer both types. Quick add adds the summary by default.
