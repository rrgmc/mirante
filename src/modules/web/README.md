# web module

Shows any web page as an item. See [item types](../../../docs/item-types.md#web-page).

## Item types

| Id | Name | Rendering | Description |
|---|---|---|---|
| `web.page` | Web page | web | The web page at an address, signed in with the item's profile. |

The module calls itself "Web" and describes itself as: "Any web page as an item." Help > Modules shows the names and descriptions. See [decision 0056](../../../docs/decisions/0056-modules-window.md).

Its UUID is `bb731c5c-9a18-4e74-81f9-ae98f1fb24d1`. It is at version 0.1.0, by Rangel Reale. Help > Modules also shows these, and the module's homepage. See [decision 0087](../../../docs/decisions/0087-module-identity.md).

Since M3 the type declares its configuration fields, so items can be created and edited. Since M4 it shows the page in a web view, through `ItemContext::createWebView`. `WebUrls.cpp` reads the address. If `url` is empty or is not an `http` or `https` address, the cell says "Set the address in the item's settings." instead.

## Items from a link

`web.page` implements `fromUrl()`, through `matchUrl()` in `WebUrls.cpp`. It takes any `http` or `https` address with a host, stores it in `url`, and names the item after the host, without `www.`. The match is generic, so the item dialog lists Web page after the types of other modules. See [decision 0049](../../../docs/decisions/0049-items-from-a-pasted-link.md). The tests are in `tests/tst_from_url.cpp`.

## Configuration

| Key | Type | Description |
|---|---|---|
| `url` | URL, required | The page's address, starting with `http://` or `https://`. |

## Settings

None.
