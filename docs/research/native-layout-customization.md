# Customizable native layouts

- Status: Research. Nothing here is decided or built.
- Date: 2026-10-31
- Qt: 6.8.3

[Decision 0066](../decisions/0066-fixed-native-layouts-with-sections.md) fixes each native layout in code, and users can only hide sections. This document asks how users could change more, and whether an HTML engine lighter than Chromium or a template language would help. It assumes that **layouts may be shared by other people**, so a layout file must be treated as hostile: it must not be able to run code, reach private data, crash the app or hang it.

No item type needs this today. If one does, record the choice as a new [decision](../decisions/README.md), and update [item types](../item-types.md) and [modules](../modules.md).

## Summary

| Way | Memory per view, at 30 views | A hostile layout can | Main limit |
|---|---|---|---|
| [Section order](#section-order) | The same as today | Do nothing | Changes order only |
| [Layout description](#a-layout-description-over-native-widgets) | About the same as native widgets (0.6 MB) | Do nothing, by design. Not built or tested | Only the elements we provide |
| [`QTextBrowser`](#qtextbrowser) | 0.6 MB | Hang the app | Tables only, no flexbox |
| [litehtml](#litehtml) | 5.6 MB | **Crash or hang the app** | Must run with strict limits; qlitehtml is GPL |
| [Qt WebEngine](#qt-webengine) | 35 MB | Use one page's renderer; the app stayed responsive | The cost, and this is Chromium |
| [QML](#qml) | Not measured | **Send data to the network** | QML files are code |
| [Sciter or Ultralight](#sciter-and-ultralight) | Not measured | Not tested | Closed source |

**Recommendation.** If this is built, use two steps:

1. **Section order.** It is cheap, and [decision 0066](../decisions/0066-fixed-native-layouts-with-sections.md) already allows it.
2. **A layout description over native widgets:** a small JSON format that names elements such as rows, text, pills and sections. A file then has no HTML, CSS or scripts for an attacker to use, and the view keeps native keyboard use, accessibility and colours.

Do not run shared HTML in litehtml or `QTextBrowser` inside the app's process. They look attractive, and litehtml's result was very close to Chromium's, but a short file stopped either of them. If shared layouts ever need real HTML and CSS, use Qt WebEngine with scripts and requests off, and accept its cost.

## What was tried

One merge request, as JSON, with 20 comments. The same layout was written once with flexbox for litehtml and Qt WebEngine, and once with tables for `QTextBrowser`. [How this was tested](#how-this-was-tested) has the details and limits.

litehtml drew the flexbox layout almost as Chromium did, including wrapping and rounded labels. Its rounded corners looked rougher, and the font weight differed slightly. `QTextBrowser` could not wrap the labels or round them.

### Cost

Memory is the private memory added by the views, including child processes. Each is the median of 3 runs. The native row is a tree of `QLabel` widgets in a `QScrollArea`, as a stand-in for `SummaryView`.

| Engine | 1 view | 10 views | 30 views | Per view, at 30 |
|---|---|---|---|---|
| Native widgets | 4.8 MB | 11.2 MB | 18.2 MB | 0.6 MB |
| `QTextBrowser` | 5.7 MB | 11.7 MB | 17.3 MB | 0.6 MB |
| litehtml | 31.0 MB | 75.5 MB | 167.1 MB | 5.6 MB |
| Qt WebEngine | 150.5 MB | 432.4 MB | 1054.1 MB | 35.1 MB |

litehtml's memory is higher than expected, and its cause was not investigated. Another painting backend may differ.

Times are medians, in milliseconds. A first paint runs from making the view to its first image. A new layout follows a change of width. For Qt WebEngine both end when a script sees the second animation frame, and the engine was already running, so a cold start takes longer.

| Engine | First paint | New layout |
|---|---|---|
| Native widgets | 41 | 12 |
| `QTextBrowser` | 21 | 7 |
| litehtml | 24 | 7 |
| Qt WebEngine | 85 | 35 |

Added program size, in a release build, compared with an almost empty Qt program:

| Library | Added size |
|---|---|
| litehtml, gumbo and qlitehtml, linked statically | 1.5 MB |
| Inja and nlohmann/json | 0.3 MB |
| Cutelee, with its four plugin libraries | 0.9 MB |

Qt WebEngine and QML add nothing, since Mirante already ships them. Qt WebEngine depends on Qt Quick, and `Qt6Qml.dll`, `Qt6Quick.dll` and `Qt6QuickWidgets.dll` are already in the release build.

### CSS that works

Each check draws boxes in known colours and reads pixels from the image. Qt WebEngine ran as the control and passed every check.

| Feature | litehtml | `QTextBrowser` |
|---|---|---|
| Flexbox: row, wrap, `justify-content` | Yes | No |
| Flexbox `gap` | No | No |
| Grid | No | No |
| Custom properties, `calc()` | Yes | No |
| `float`, `position: absolute`, `inline-block` | Yes | No |
| Rounded corners | Yes | No |
| Tables | Yes, but `height` on a cell is ignored | Yes, with its own [subset](https://doc.qt.io/qt-6.8/richtext-html-subset.html) |
| `<body>` background | **Not painted** | Yes |
| Media queries | Wrong through qlitehtml: `max-width: 1px` matched | No |

Most `QTextBrowser` results use the same boxes, and an empty block may collapse there, so read them with Qt's subset list.

In litehtml, a `<div>` with a background does paint it, so a layout needs one around its content. In a dark theme, text without a colour is black on a dark background: the engine does not read Qt's palette.

## The options

### Section order

Each summary already builds its sections through [`SummaryView`](../../src/module_api/mirante/widgets/SummaryView.h), which keeps the order of first use. An item could store an order, and `SummaryView` could follow it.

The item dialog builds its form from configuration fields, so this would probably need a new kind of field. It carries no risk, because the configuration holds only known section names.

### A layout description over native widgets

A layout would be plain data, read by a small part of the core that builds widgets:

```json
{
  "format": 1,
  "itemType": "gitlab.mr.summary",
  "layout": [
    { "row": [
      { "pill": "state" },
      { "pill": "pipeline.status" },
      { "text": "updated", "style": "muted" }
    ]},
    { "action": "retryPipeline" },
    { "section": "People", "items": [
      { "field": "author" },
      { "list": "approvals" }
    ]},
    { "section": "Comments", "items": [ { "comments": "comments", "limit": 5 } ] }
  ]
}
```

**This format was not built or tested.** The native row of the cost table is its nearest measure. Its value comes from what it leaves out:

- **Nothing to run.** No scripts, no CSS, no parser for HTML. The core reads JSON with Qt's own parser and refuses what it does not know.
- **Limits are easy.** The reader counts depth and elements as it goes, and stops. A list has a fixed upper limit. Time grows only with the size of the file, which also has a limit.
- **No forged actions.** `retryPipeline` is a name with no address and no arguments. The item supplies its own pipeline. See [actions](#actions).
- **Native behaviour.** Labels and links keep the keyboard, accessibility, palette, and the translated names of priorities and states, with no work per layout.
- **Existing parts.** `FlowRow`, `StatusPill`, `ActionRow`, `CompactDate` and `MarkdownView` already exist in the [module API](../modules.md).

Its limit is that it offers only the elements we write. Someone cannot draw a new kind of chart, for example. A later format version can add elements.

### `QTextBrowser`

It adds no engine, and [`MarkdownView`](../../src/module_api/mirante/widgets/MarkdownView.h) already wraps it. It has a text interface for accessibility. It suits text more than layout.

It is not safe for hostile files. A table nested 30 deep, 100,000 nested `<div>`s or 1,000,000 `<span>`s each held the main thread for more than 30 seconds. By default it also reads an image from any `file:` address, which a viewer must refuse by overriding `loadResource()`, as `MarkdownView` does.

### litehtml

[litehtml](https://github.com/litehtml/litehtml) is a BSD-licensed layout engine with no scripts. It parses with gumbo and asks its host to draw and to supply fonts. Text selection, link clicks, wheel scrolling, page keys and a 2× scale all worked.

**The problems:**

- **Licence.** [qlitehtml](https://code.qt.io/cgit/playground/qlitehtml.git/about/), the Qt painting backend and widget, is under GPL 3 or a Qt commercial licence. Mirante is [MIT](../../LICENSE). Using litehtml means writing our own `document_container`, which draws through `QPainter`, or changing Mirante's licence. A check of the licences by someone qualified is needed either way. This also means that the numbers here come from code we could not ship.
- **Crashes and hangs.** See [hostile files](#hostile-files). They happen on the main thread, so they stop or end the whole app.
- **A missing resource handler ends the program.** A page with an `<img>` and no handler set ended with `0xC0000409`. A view must always set one.
- **Copying.** The widget selects text but does not handle Ctrl+C. The host must do so.
- **Keyboard.** Tab does not move to a link, and Enter does not open it. The widget handles only scrolling keys.
- **Accessibility.** A screen reader gets a scroll area with no text interface. The text of the layout is not exposed. `QTextBrowser` does expose one.

### Qt WebEngine

Mirante already has it, and it is the only option here that kept the app responsive with every hostile file. The longest gap on the main thread was 48 ms, while the renderer process worked. It also has full CSS.

With no restrictions, a page loaded everything from the test server and ran its script. With `JavascriptEnabled` off and a `QWebEngineUrlRequestInterceptor` that blocks all but `data:` and `about:`, it made no connection and ran no script. [Module web content](module-web-content.md) describes how a module could serve such a page.

Its cost is about 60 times that of a native view at 30 views.

### QML

A QML file is code, so it does not fit shared layouts. In the test, a QML file from a string, with no special permission, sent `GET /qml-xhr?stolen=value` to a local server with `XMLHttpRequest`. Reading a local file with `XMLHttpRequest` returned nothing, because Qt turns that off by default.

A custom `QQmlNetworkAccessManagerFactory` might block the network, but this was not tested. Scripts would still run on the main thread, so a loop could hang the app.

If layouts were only ever the user's own, QML would be worth a new look, since it adds no size. It would need tests of: cost per `QQuickWidget`, how it looks beside widgets, how much the network block covers, and what files it could import. [Decision 0016](../decisions/0016-generic-items.md) keeps it as a possible addition, and the synced data folder would need thought, because a file on one device would then run as code on another.

### Sciter and Ultralight

Both are closed source for most users, and neither was tested. [Sciter's free version](https://sciter.com/prices/) must be used as the published binaries; that page was last updated in 2022. [Ultralight](https://ultralig.ht/pricing/) is free below $200,000 of yearly turnover and of funding, and only its Enterprise tier includes source. A bug like those in [hostile files](#hostile-files) could not be fixed by us, so they are not a good base for this.

### Lists

Lists should stay native. `ItemListView` is a sortable table, with sorting and narrow-cell rules that a template would have to rebuild. Their options should be columns: which ones to show, and in what order, as the merge request list has begun with [decision 0066](../decisions/0066-fixed-native-layouts-with-sections.md).

## Template languages

A template language is only needed for the HTML options. A layout description has no text to fill in.

| Engine | Syntax | Licence | Added size | Note |
|---|---|---|---|---|
| [Inja](https://github.com/pantor/inja) 3.5.0 | Like Jinja | MIT | 0.3 MB | Used and tested here. Needs nlohmann/json, so `QJsonObject` must be converted |
| [Cutelee](https://github.com/cutelyst/cutelee) 6.2.0 | Like Django | LGPL 2.1 | 0.9 MB | Built with Qt 6.8.3, but not run. Needs CMake 3.30, while Mirante asks for 3.21; the build used a copy that asked for 3.25. Loads filters and tags as plugin libraries. Has a `range` tag and a `load` tag, which would need review |
| Mustache | No logic | | Not measured | Compared on paper. Small enough to write on `QJsonValue`, with a limit on work. Every condition must come ready in the data, which [decision 0095](../decisions/0095-the-bridge-gives-texts-and-colours.md) already does for texts |

Rendering the sample with Inja took about 0.1 ms, so speed does not separate them.

**Inja is not safe for shared templates as it stands:**

| Check | Result |
|---|---|
| Escaping by default | **Off.** `{{ title }}` wrote `<img src=x onerror=alert(1)>` as it was |
| With `set_html_autoescape(true)` | Strings are escaped in values, attributes, loops, `at()`, `join()` and `upper()` |
| Printing a whole list or object with autoescape on | **Not escaped.** `{{ list }}` wrote `["<b>"]` |
| A value that looks like a template | Stays text: `{{ nested }}` wrote `{{ token }}` |
| Files in `{% include %}` by default | **On.** It read a local file into the output |
| With `set_search_included_templates_in_files(false)` | The include fails with an error |
| Three nested `range(100000)` loops | **Did not end within 30 s.** Inja has no limit on work that the test found |
| Two nested loops that write text | The program ended after 27 s, probably at the test's 4 GB limit |
| 100,000 nested `{% if %}` or parentheses | **Stack overflow.** 1,000 nested `{% if %}` were fine |

The unescaped list matters even though it looks small. Text from a server, such as a label, could then carry a link to an action. A safe use would escape every string in the data first, and also remove `range`, or write a small engine with a limit on work.

## Hostile files

Each case ran alone, on the main thread as a view would, with 30 s and 4 GB limits. The stack overflow is an error that ends the program at once: it uses up the memory for nested calls. On Windows the main thread has 1 MB for this by default, and Mirante does not change it.

| Page | litehtml | `QTextBrowser` | Qt WebEngine |
|---|---|---|---|
| Nested `<div>` | 800 fine, **900 crashes** | 1,000 fine, 100,000 held it > 30 s | 100,000 in 1.4 s |
| Nested flexbox | 10: 2 s, 11: 8 s, **12: > 30 s** | 30: 4 ms, since it ignores flexbox | 30: 0.6 s |
| Nested tables | 12: 2 s, 14: 19 s, **15: > 30 s** | **30: > 30 s**; limit not narrowed | 30: 0.5 s |
| A selector of `div ` words, over 50 nested `<div>`s | 25: 1 s, 30: 26 s, **50: > 30 s** | 5,000: 9 ms | 5,000: 0.6 s |
| Table rows | 20,000: 3 s, 100,000: > 30 s | 100,000: 10 s | 100,000: not painted in 20 s, app responsive |
| `<span>`s | 50,000: 14 s, 100,000: > 30 s, 1,000,000: ended | 1,000,000: > 30 s | 1,000,000: not painted in 20 s, app responsive |

The time for nested flexbox and tables grows about three to four times with each level, so a limit must be low. The flexbox, table and selector files are under 1 kB, so a limit on file size does not help. A very wide box, a very large font, a huge border and a `colspan` of 1,000,000 did no harm in litehtml.

To use litehtml with shared layouts, the core would have to parse the HTML first, and refuse any file beyond limits on depth, flexbox and table nesting, selector length and number of elements. This is possible with gumbo, but a list of limits proves only that the known cases are blocked. A real guard would be a separate process, which gives up much of what makes litehtml light.

### Requests and files

A page asked for a stylesheet, an `@import`, a background, images, a script, `fetch`, an `<iframe>`, an `<object>` and a `<video>` from a local test server.

| Engine | Connections to the server | Script ran | Other |
|---|---|---|---|
| litehtml | None | No | The resource handler was asked for the stylesheets, background and images, including a `file:` address. It must refuse them |
| `QTextBrowser` | None | No | Reads a `file:` image by default |
| Qt WebEngine, default | All 10 | Yes | |
| Qt WebEngine, scripts off and all requests blocked | None | No | |

## What any custom layout needs

These apply to every option except section order.

- **A data contract per item type.** A layout reads named values, not GitLab's or Jira's JSON. Otherwise a change on a server breaks layouts, and an API token could slip in. The data must never hold a token, cookie or local path.
- **Translated values and colours.** Values come formatted and translated, as in [decision 0095](../decisions/0095-the-bridge-gives-texts-and-colours.md) and [decision 0086](../decisions/0086-english-and-brazilian-portuguese.md). A layout's own fixed text, such as a section title, needs a rule: either one language, or named texts from the app. The layouts that ship with the app must be translated.
- **Falling back.** A layout that fails to read shows a short message and the built-in layout, so an item is never empty.
- **Storing and sharing.** A layout could be a file in the workspace, which syncs like other data, and an import and export command. Files need a format number, and [decision 0074](../decisions/0074-refuse-workspaces-from-newer-versions.md) is a model for files from a newer version. An imported layout should be a visible, deliberate step.

### Actions

[Decision 0070](../decisions/0070-read-mostly-native-views.md) keeps only CI actions in native views, and those change real pipelines. A shared layout could show a link named "Open details" that retries a pipeline.

- An action has a name and no arguments. The item supplies the pipeline or job, so a layout cannot name another one.
- The confirmation text comes from the app and the item: the action, the project and the pipeline. It never comes from the layout.
- A layout chooses a known action, and the app draws its button with its normal name.
- Links to the outside open only `http` and `https`, and show the real address when hovered.

## Open questions

- Does a layout description cover what people want? Try it on the merge request, issue and Jira summaries before designing its format.
- Who needs what the format cannot do, and is that a real need? Asking would be better than building the HTML options in advance.
- Should a layout be per item, per item type or per profile?
- How should the item dialog show the choice of layout and section order?
- Why does litehtml use 5.6 MB per view?

## How this was tested

A small throwaway program, on Windows 11 with Qt 6.8.3, MSVC 2022 and a release build. It was not kept in the repository. It used:

| Source | Commit or version |
|---|---|
| qlitehtml, with its bundled litehtml and gumbo | litehtml at `9bc84b8` |
| Inja, with its bundled nlohmann/json | 3.5.0, at `482473a` |
| Cutelee | 6.2.0, built only |

Limits of the test:

- Windows only. Linux and macOS were not run. Their main threads usually have more stack, so the crash limits will differ, but the slow cases should not.
- Memory, times, CSS checks, screenshots, Qt WebEngine's hostile files and litehtml's narrowed limits used real windows, on one screen, without taking focus. The memory includes the window's own image, and a window of 30 views was probably larger than the screen. Requests, escaping and behaviour ran on Qt's offscreen platform, and so did the `QTextBrowser` hostile files, Inja's, and litehtml's largest cases.
- Offscreen has no real fonts and cannot grab a Qt WebEngine image, so a first run of those checks was discarded.
- Selection, links and keys were sent as test events, not by a person. Accessibility was read through Qt's `QAccessible` interfaces, and no screen reader was used.
- The clipboard check used the offscreen clipboard. It worked for a direct write, and Ctrl+C put nothing there. The widget's source agrees: it handles no copy key.
- The litehtml in qlitehtml (`9bc84b8`) is not the same commit as the current upstream (`3a1f84d`), and upstream was not tested. A hostile case may behave differently there.
- The layout description, a custom litehtml backend and a limit on template work are not built. Claims about them come from reasoning, and are marked as such.
- The Sciter and Ultralight terms were read on their sites, and they can change.
