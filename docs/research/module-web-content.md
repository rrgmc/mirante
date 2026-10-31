# Web content that a module bundles

- Status: Research. Nothing here is decided or built.
- Date: 2026-10-31
- Qt: 6.8.3

This document asks how a [module](../modules.md) could show a web page that it serves itself, with its files and data coming from the module's own code. It compares three ways, and reports what each one did in a test.

No module needs this today. If one does, record the choice as a new [decision](../decisions/README.md), and update [web views](../web-views.md) and [modules](../modules.md).

## Summary

| Way | Core change | Good for | Main limit |
|---|---|---|---|
| [`qrc:` address](#1-a-qrc-address) | None | Static files | `fetch` fails, and every page shares one origin |
| [Local server](#2-a-local-server) | None | A real server program, cookies, WebSocket | A port that any local program can reach |
| [Scheme handler](#3-a-scheme-handler) | Small | Files and data served from code, with no port | No cookies and no HTTP status codes |

**Recommendation.** If a module needs this, build the scheme handler, with one scheme for all modules and a host per module. Use a `qrc:` address for a page that needs no data from the module. Use a local server only when the content needs a real server.

An origin, used often below, is the scheme and host of a page. Browsers keep the storage of each origin apart.

## 1. A `qrc:` address

A module already compiles its `RESOURCES` in as `:/modules/<id>/<path>`. A page can load them, with no code in the module:

```cpp
context.createWebView(QUrl("qrc:/modules/mymodule/index.html"), parent);
```

Measured:

- The page and its relative scripts load in a persistent profile. The page is a secure context.
- `fetch` fails, even for the page's own files: `URL scheme "qrc" is not supported`.
- `localStorage` works and survives a restart.
- The origin is `qrc:` for every path. All `qrc:` pages share one storage, so two modules can read each other's.
- A page of `https://example.com` in the same profile can load a `qrc:` file with a `<script>` tag, but cannot `fetch` it. Bundled files are therefore not private. This already applies to all of Mirante's resources.
- A `qrc:` page cannot load a script from a local file or from a local custom scheme.

Without `fetch` or a server, the page has no way to get data from the module. A module that builds its own page from `profile()` could add a `QWebChannel`, but it then handles popups, permissions and downloads itself; see [decision 0037](../decisions/0037-core-makes-web-views.md).

## 2. A local server

The module starts a server on `127.0.0.1`, for example with `QTcpServer` and `QT_LIBS Network`, or starts a program with `QProcess`. It then calls `createWebView()` with the server's address. This needs no core change. It was not run in the test, so these points come from the code and from how browsers work:

- It behaves like the web, so cookies, WebSocket and existing web frameworks work.
- Any program on the machine can connect to the port, so the server must require a random token.
- `localStorage` is kept per port, so a random port loses it at each start. Cookies are not kept per port, so all `127.0.0.1` servers in a profile share them.
- The module manages the server's life. Views come and go with [placements](../web-views.md#placements-and-live-copies) and the [memory policy](../web-views.md#memory-policy), so the server should not belong to a view.

## 3. A scheme handler

Qt WebEngine lets code answer the requests of a custom scheme directly, with no socket and no port. A class derived from `QWebEngineUrlSchemeHandler` receives each request as a job, and answers with any `QIODevice`. [`DashSchemeHandler`](../../src/web/DashSchemeHandler.cpp) does this for `dash://`.

```cpp
void Handler::requestStarted(QWebEngineUrlRequestJob *job)
{
    auto *buffer = new QBuffer(job); // The job owns it, so it lives as long as the job.
    buffer->setData(contentFor(job->requestUrl()));
    buffer->open(QIODevice::ReadOnly);
    job->reply("text/html", buffer);
}
```

### What the page can do

Measured with `Syntax::Host` and the flags `SecureScheme`, `LocalScheme` and `FetchApiAllowed`. Most rows also had `CorsEnabled`. Only the `fetch` of a page's own files and `localStorage` were also checked without it:

| Check | Result |
|---|---|
| Page, relative scripts, secure context | Work |
| `fetch` of its own files | Works, but only with `FetchApiAllowed`; without it the scheme is "not supported" |
| `POST` from `fetch` or `XMLHttpRequest` | Works. `requestBody()` gives a closed device: open it first, or it reads as empty |
| Streaming | Works. A device that gave 5 chunks 200 ms apart reached the page's `fetch` reader at about the same times |
| A 50 MB reply from a `QBuffer` | Took 122 ms on the test machine |
| `localStorage` | Works and survives a restart. It is kept per host |
| Cookies | Do not work, from a script or from a `Set-Cookie` header |
| A job that is never answered | The `fetch` still waited after 3 s, so a handler must always answer |

The API has no call to set an HTTP status, so a data call must report its errors in its body. WebSocket is a separate scheme and a handler does not serve it. `EventSource` was not tested.

### Threads

`requestStarted()` ran on the main thread. Qt read the reply device on another thread: the streaming test counted 12 reads away from the main thread. The [Qt documentation](https://doc.qt.io/qt-6.8/qwebengineurlrequestjob.html#reply) says the same, so a device that the main thread still writes to needs a lock.

### When to register the scheme

| Scheme registered | Result |
|---|---|
| Before `QApplication`, as `dash` does | Correct origin, secure context |
| After `QApplication`, before any Qt WebEngine object | Correct origin, secure context |
| After the first Qt WebEngine object | Ignored. The page loads, but its origin has no host, and it is not a secure context |
| Never, with a handler installed | The same as ignored |

Modules register in `main.cpp` after `QApplication` and before `ProfileManager`, so the second row would allow a scheme per module. The [Qt documentation](https://doc.qt.io/qt-6.8/qwebengineurlscheme.html#registerScheme) only says "generally before" the application object, so this is not a promise. One scheme registered beside `dash` avoids the question.

A scheme applies to all profiles, but a handler belongs to one. A profile without the handler fails the address. A handler for `http` is refused, so it cannot stand in for a real address.

### What other pages can reach

| Request | From a site in the profile (`https://example.com`) | From another host or scheme of a module |
|---|---|---|
| `fetch` | Blocked before the handler | Reaches the handler |
| `<script>` | Blocked | Reaches the handler |
| `<iframe>` | Blocked | Loads, but its document cannot be read |
| Going to the address | Blocked | Not tested |

`LocalScheme` gives the left column. Without it, as in a test scheme with `SecureScheme`, `CorsEnabled` and `FetchApiAllowed`, a site could `fetch` the content and run its scripts. Every scheme that serves module content must set it.

The right column shows that browser rules do not keep modules apart. Leaving out `CorsEnabled` did not change this. The handler can do it itself: `job->initiator()` gave the origin of the page that asked, and was empty for a top-level load.

A handler that refused other origins with `job->fail()` stopped the `<script>` test. The `fetch` test had no answer after 10 s, though, and this was not investigated. Find out why before relying on it.

### Local files

By default, a page of a local scheme can load a script from a `file:` address, because `LocalContentCanAccessFileUrls` is on. Turning that setting off is not a fix, because it also blocked the page's own script. A `Content-Security-Policy` header works, as `dash://` already does: with `default-src 'self'; connect-src 'self'`, the page could not load the file, could not `fetch` another scheme, and could not run inline scripts.

## A design for the scheme handler

This is a starting point for a decision, not a plan.

- **One scheme, with a host per module:** `module://<module id>/<path>`, registered beside `dash` in `main.cpp`. Each module gets its own origin and storage, without any change to when modules register. The flags would be `SecureScheme`, `LocalScheme` and `FetchApiAllowed`, and `POST` and streaming should be tested again without `CorsEnabled`.
- **The core owns the handler.** It checks that the host is an enabled module, refuses requests from other origins, sets the security headers, and calls the module. A module keeps to the Qt types it has today, so it does not link Qt WebEngine and its tests can use a fake, as [decision 0037](../decisions/0037-core-makes-web-views.md) wants.
- **A new member of `ModuleRegistrar`,** beside `addSettingsScreen()` and the others in [Module.h](../../src/module_api/mirante/Module.h). It takes an object that answers a request. The core could serve `:/modules/<id>/web/` by default, so that static files need no code.
- **One handler per profile.** `ProfileManager::profile()` installs it. A handler then knows its profile, so a module's answer can use that profile's settings and server. A profile that is [cleared](../web-views.md#profiles) is made again, so the handler must be installed again with it. The app profile and the temporary profile used during clearing stay without it.
- **Headers by default,** like those of `dash://`, with `'self'` in place of `dash:`. A module could relax them only for its own content.

Other code that assumes web addresses must change:

- The toolbar's "open in the external browser" ([WebPanel.cpp:88](../../src/web/WebPanel.cpp:88)) would hand `module://` to the system, which cannot open it. It should be hidden for these pages.
- A link that opens a new window goes to the external browser ([WebPage.cpp:89](../../src/web/WebPage.cpp:89)), so it has the same problem. Such links should open inside the item, or do nothing.

Open questions:

- What is the right answer to a refused `fetch`, given the result under [what other pages can reach](#what-other-pages-can-reach)?
- How does a page know which item it belongs to? Placing the item id in the path is simple, but the core must then check it.
- Should data calls have a small API, such as JSON over `fetch`, so that modules answer them alike?

## How this was tested

Three small throwaway programs, on Windows 11 with Qt 6.8.3 and a release build. They were not kept in the repository. Each loaded pages in a persistent `QWebEngineProfile` and read the results with `runJavaScript()`.

Limits of the test:

- It ran on the offscreen platform, with Chromium's sandbox turned off. The sandbox could affect file access in particular.
- Pages were plain `QWebEnginePage` objects, not Mirante's `WebPage` and `WebPanel`, so nothing was tried inside the app.
- Linux and macOS were not tested. The results depend only on Qt API, so they should match, but this needs checking; CI could run it on Linux.
- The site page had the origin of `https://example.com` through `setHtml()`, with no network. Access from a page of a local scheme to a real `https` address was not tested.
- `setHtml()` was also tried, for a single page without a scheme: 1 MB loaded and 3 MB failed. The [Qt documentation](https://doc.qt.io/qt-6.8/qwebenginepage.html#setHtml) gives a limit of about 2 MB.
