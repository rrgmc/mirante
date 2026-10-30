// What every built-in panel shares: the connection to the app's bridge
// "dash", the app's colours and font, and small helpers that build the page.
// A panel calls dash.start(render). render(bridge, texts) draws the whole
// page, and is called again whenever the bridge says something changed, and
// once a minute, so ages such as "5 min ago" stay right.
// See docs/web-views.md#built-in-panels.
"use strict";

const dash = (() => {
    // A bridge method as a promise: call(bridge.itemStatus, id).
    function call(method, ...args) {
        return new Promise((resolve) => method(...args, resolve));
    }

    // The app's colours and font, as CSS custom properties: --window, --text
    // and so on.
    function applyPalette(palette) {
        const style = document.documentElement.style;
        for (const [key, value] of Object.entries(palette)) {
            if (key === "fontFamily" || key === "fontSize")
                continue;
            style.setProperty("--" + key, value);
        }
        if (palette.fontFamily)
            style.setProperty("--font-family", `"${palette.fontFamily}", system-ui, sans-serif`);
        if (palette.fontSize > 0)
            style.setProperty("--font-size", palette.fontSize + "px");
    }

    // An element with a class and text. Text is always set as text, never
    // as markup.
    function element(tag, className, text) {
        const node = document.createElement(tag);
        if (className)
            node.className = className;
        if (text !== undefined && text !== null && text !== "")
            node.textContent = text;
        return node;
    }

    // A row that opens something on a click, or on Enter or Space.
    function clickable(node, open) {
        node.tabIndex = 0;
        node.setAttribute("role", "button");
        node.addEventListener("click", open);
        node.addEventListener("keydown", (event) => {
            if (event.key === "Enter" || event.key === " ") {
                event.preventDefault();
                open();
            }
        });
        return node;
    }

    // The page's content, replaced in one step so the page never shows half
    // a render.
    function show(...nodes) {
        const root = document.getElementById("root");
        root.replaceChildren(...nodes);
    }

    function message(text) {
        return element("p", "message", text);
    }

    function start(render) {
        new QWebChannel(qt.webChannelTransport, (channel) => {
            const bridge = channel.objects.dash;
            let busy = false;
            let again = false;
            async function refresh() {
                if (busy) {
                    again = true;
                    return;
                }
                busy = true;
                try {
                    applyPalette(await call(bridge.palette));
                    await render(bridge, await call(bridge.texts));
                } catch (error) {
                    console.error(error);
                }
                busy = false;
                if (again) {
                    again = false;
                    refresh();
                }
            }
            bridge.changed.connect(refresh);
            setInterval(refresh, 60 * 1000);
            refresh();
        });
    }

    return { call, element, clickable, show, message, start };
})();
