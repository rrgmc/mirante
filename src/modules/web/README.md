# web module

Shows any web page as an item. See [item types](../../../docs/item-types.md#web-page).

## Item types

| Id | Name | Rendering |
|---|---|---|
| `web.page` | Web page | web |

Since M3 the type declares its configuration fields, so items can be created and edited. Since M4 it shows the page in a web view, through `ItemContext::createWebView`. `WebUrls.cpp` reads the address. If `url` is empty or is not an `http` or `https` address, the cell says "Set the address in the item's settings." instead.

## Items from a link

`web.page` implements `fromUrl()`, through `matchUrl()` in `WebUrls.cpp`. It takes any `http` or `https` address with a host, stores it in `url`, and names the item after the host, without `www.`. The match is generic, so the item dialog lists Web page after the types of other modules. See [decision 0049](../../../docs/decisions/0049-items-from-a-pasted-link.md). The tests are in `tests/tst_from_url.cpp`.

## Configuration

| Key | Type | Description |
|---|---|---|
| `url` | URL, required | The page's address, starting with `http://` or `https://`. |

## Settings

None.
