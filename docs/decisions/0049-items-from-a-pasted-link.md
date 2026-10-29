# 0049. Item types read a pasted link, and the item dialog offers the types that take it

- Status: Accepted; the connections are the profile's by [0050](0050-profiles-hold-logins-connections-and-settings.md)
- Date: 2026-10-29

## Context

Many items show something that has a web address: a merge request, an issue, a Jira issue, a Claude session. The user usually has that address in the browser. To make an item of it, the user had to pick the type, then copy the project path and the number from the address into the form by hand. Types that are not web pages, such as "GitLab Issue (summary)", need the same values. The [roadmap](../roadmap.md) planned "Pasting a URL into the item dialog picks the right type and fills its fields" for M6, and [decision 0043](0043-pick-the-item-type-first.md) left room for it on the type page.

Only the module knows how its addresses are built. The core must not parse GitLab or Jira addresses. Self-hosted servers have their own host, and sometimes a path, such as `https://example.org/gitlab/`, so the module needs the base URLs of the group's connections to read them.

## Decision

1. **An optional method on `ItemType`.** `fromUrl(url, baseUrls)` returns a `UrlMatch`, or nothing, the default. A match holds the configuration that shows the address, a suggested item name, the base URL of the server the address belongs to, and a flag for generic types that take any address. `baseUrls` are the base URLs of the group's connections of the type's connection kinds. `UrlMatch` and a helper, `pathBelow()`, live in the new module interface header `UrlMatch.h`. A type maps the address only to fields it already has.
2. **A link field on the type page.** The first page of the new item dialog has a field above the grid: "Paste a link to create an item from it". While it holds an http or https address, the grid shows only the types whose `fromUrl()` takes it: the types of modules first, then generic types such as Web page, each group by name. The best match is selected, so Enter goes on with it. Choosing a type fills its configuration, over its defaults, and the name, unless the user typed one. Clearing the field shows every type again.
3. **The connection of the link's server.** If the group has a connection, of one of the type's kinds, whose base URL equals the match's base URL, the new item uses it. The item's `connectionId` is set only if the [resolution order](../data-model.md#connection) would pick another connection.
4. **"Paste URL…" in the menus.** The context menu of a task and of its Items row, next to "New item…", has "Paste URL…". It opens the new item dialog with the link from the clipboard in the field. It is disabled while the clipboard holds no web address.

## Alternatives

- **A separate "Paste URL" dialog.** It would repeat the type grid and the editor. The type page already has both, and the field fits above the grid.
- **Pick the best type without asking.** A merge request's address fits the summary, the page and the CI pipeline equally well. Only the user knows which view is wanted, so the dialog lists them and selects the first.
- **Give `fromUrl()` the whole connection, or the item context.** The module needs only the base URLs to read an address. The core matches the base URL back to a connection, so modules see no connection ids.
- **A rank number on each match.** Two levels, module types and generic types, cover every type so far. A number can be added to `UrlMatch` later.
- **Quick add.** Quick add, planned for M7, makes a whole task from pasted text. Making one item in an existing task is a smaller step and comes first. Quick add handlers can use `fromUrl()` later.

## Consequences

- Making an item from a browser address takes a paste and Enter.
- Every module that has web addresses should implement `fromUrl()` and test it with good and bad addresses.
- A server without a connection is still read where its addresses are distinctive, such as GitLab's `/-/` and Jira's `/browse/`. The item then has no connection until the group gets one.
- The pasted address is not stored. The item keeps only the configuration made from it.
