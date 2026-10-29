# web module

Shows any web page as an item. See [item types](../../../docs/item-types.md#web-page).

## Item types

| Id | Name | Rendering |
|---|---|---|
| `web.page` | Web page | web |

Since M3 the type declares its configuration fields, so items can be created and edited. Since M4 it shows the page in a web view, through `ItemContext::createWebView`. `WebUrls.cpp` reads the address. If `url` is empty or is not an `http` or `https` address, the cell says "Set the address in the item's settings." instead.

## Configuration

| Key | Type | Description |
|---|---|---|
| `url` | URL, required | The page's address, starting with `http://` or `https://`. |

## Connection kinds

None.

## Settings

None.
