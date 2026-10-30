# 0099. The Linux package installs into `/opt/mirante`, with its own Qt and an AppArmor profile

- Status: Accepted
- Date: 2026-10-30

## Context

The `.deb` package targets Ubuntu 24.04 and later. Mirante needs Qt 6.8, and Ubuntu 24.04 ships Qt 6.4. Qt WebEngine also starts a sandboxed helper process. Ubuntu 23.10 and later block the unprivileged user namespaces that Chromium's sandbox needs, unless an AppArmor profile allows them for that program. CI turns the sandbox off for this reason ([ci.yml](../../.github/workflows/ci.yml)), but an installed app should keep it.

## Decision

- The package installs the app with its own copy of Qt in `/opt/mirante`, as Qt's deploy script lays it out: `bin`, `lib`, `plugins`, `libexec`, `resources` and `translations`, with a `qt.conf`.
- It also installs a link `/usr/bin/mirante`, a desktop file in `/usr/share/applications` and the icon in `/usr/share/icons/hicolor/scalable/apps`, so the app appears in the desktop's menu.
- It installs an AppArmor profile, `/etc/apparmor.d/mirante`, that allows user namespaces for the app and for `QtWebEngineProcess`. Its `postinst` script loads the profile, and `postrm` unloads it. This is what Ubuntu does for other Chromium-based apps.
- `dpkg-shlibdeps` works out the package's dependencies on system libraries, looking into the bundled Qt for the libraries it links.
- The package is built on Ubuntu 24.04, for 64-bit x86.

## Alternatives

- **Depend on Ubuntu's Qt packages.** Their Qt is too old.
- **Turn the sandbox off in the installed app.** The app shows web pages from the internet, so it should keep Chromium's sandbox.
- **AppImage or Flatpak.** The roadmap asks for a `.deb`. Either can be added later, from the same install.
- **Install into `/usr`.** Bundled Qt libraries there could clash with the system's.

## Consequences

- The package is large, mostly Qt WebEngine, and does not get Qt's security updates from Ubuntu. A new Mirante release is needed for that.
- The AppArmor profile uses the ABI of AppArmor 4, which Ubuntu 24.04 has. Older Ubuntu releases are not supported.
- Other distributions can unpack the package, but only Ubuntu is checked.
