# Application Title Bar

[![GPLv3 License](https://img.shields.io/badge/License-GPL%20v3-yellow.svg)](https://opensource.org/licenses/)
![GitHub Release](https://img.shields.io/github/v/release/antroids/application-title-bar)

### NOTE: Plasma 6.3 - 6.3.3 Compatibility issues
A Drag action handler does not work properly on Plasma 6.3 - 6.3.3 with ButtonsRebind KWin plugin enabled.
The plugin can be disabled with the following command:
```bash
kwriteconfig6 --file ~/.config/kwinrc --group Plugins --key buttonsrebindEnabled false && qdbus6 org.kde.KWin /Plugins UnloadPlugin "buttonsrebind"
```
The names of `kwriteconfig6` and `qdbus6` utilities can be different for your distribution.

To enable the plugin again:
```bash
kwriteconfig6 --file ~/.config/kwinrc --group Plugins --key buttonsrebindEnabled true && qdbus6 org.kde.KWin /Plugins LoadPlugin "buttonsrebind"
```

## Description

KDE plasmoid compatible with Qt6 with window title and buttons.
I like minimalistic display layout and used Active Window Control plasmoid, but it's abandoned for several years and now incompatible with Plasma6.
So, I decided to create my own widget with the minimal set of features.

<img src="docs/img/AllInOne.png" />

### Goal

Stable and fast widget with control buttons and window title, ideally with the same functionality as Unity panel.
I would like to keep the widget pure QML to avoid incompatibility and maintenance issues.

Disadvantages of pure QML widget:
* Only icons can be used from Aurorae themes, the rest is ignored. Binary themes are unsupported at all (Issues [#18](https://github.com/antroids/application-title-bar/issues/18), [#6](https://github.com/antroids/application-title-bar/issues/6)).
* I cannot see the way to build menu with current plasmoid API (Issue [#13](https://github.com/antroids/application-title-bar/issues/13))

### Features

* Close, minimize, maximize, keep below/above buttons.
* Title with app name.
* Configure actions on mouse events.
* Configurable elements set and order.
* Different theming options. Internal Breeze icons, System icons and Aurorae theme.
* Configurable layout and geometry.
* Click and drag widget to reposition window (as if you'd dragged the window's integrated title bar)

## Installing

There are **two flavours** of the widget:

| Flavour | App Menu support | Install method |
|---|---|---|
| **QML-only** `.plasmoid` | ❌ | `kpackagetool6`, KDE Store, Nix |
| **With C++ plugin** tarball | ✅ Embedded app-menu bar | Pre-built tarball or build from source |

---

### Option A — QML-only (no embedded app menu)

Use this if you only need window title + control buttons and don't require the
embedded application menu bar feature.

#### 1. One-liner bash script

```bash
# Fresh install
wget https://github.com/antroids/application-title-bar/releases/latest/download/application-title-bar.plasmoid \
  -O ${TMPDIR:-/tmp}/application-title-bar.plasmoid && \
kpackagetool6 -t Plasma/Applet -i ${TMPDIR:-/tmp}/application-title-bar.plasmoid && \
systemctl --user restart plasma-plasmashell.service
```

```bash
# Update existing install
wget https://github.com/antroids/application-title-bar/releases/latest/download/application-title-bar.plasmoid \
  -O ${TMPDIR:-/tmp}/application-title-bar.plasmoid && \
kpackagetool6 -t Plasma/Applet -u ${TMPDIR:-/tmp}/application-title-bar.plasmoid && \
systemctl --user restart plasma-plasmashell.service
```

#### 2. Plasma UI
- **KDE Store:** "Add Widgets..." → "Get New Widgets..." → "Download..." → search *Application Title Bar*
- **Direct link:** [KDE Store page](https://store.kde.org/p/2135509)
- **Local file:** download the latest `application-title-bar.plasmoid` from the
  [Releases page](https://github.com/antroids/application-title-bar/releases) then
  "Add Widgets..." → "Get New Widgets..." → "Install Widget From Local File"

---

### Option B — With embedded app menu (C++ plugin)

This variant adds an **Application Menu Bar** element you can place in the
widget. When "Show app menu on hover" is enabled in settings, hovering the
window title smoothly reveals the app's native menu bar — no extra panel applet
needed.

The plugin speaks the `com.canonical.dbusmenu` D-Bus protocol directly, so it
works with GTK, Qt5, Qt6 and Electron apps that export a global menu.

#### From the pre-built release tarball

1. Download `application-title-bar-x86_64.tar.gz` from the
   [Releases page](https://github.com/antroids/application-title-bar/releases).
2. Extract and run the bundled install script:

```bash
tar -xzf application-title-bar-x86_64.tar.gz
sudo bash install.sh
systemctl --user restart plasma-plasmashell.service
```

The script:
- copies `libapplicationtitlebar.so` and `qmldir` into the Qt6 QML import path
  (`$(qmake6 -query QT_INSTALL_QML)/org/kde/applicationtitlebar/`)
- installs the Plasma package with `kpackagetool6`

#### Building from source

**Requirements:** `cmake`, `extra-cmake-modules`, `Qt6` (Core, Gui, Widgets,
DBus, Quick), `ninja` (optional but recommended).

On **Debian / Ubuntu 24.04+:**
```bash
sudo apt-get install \
  build-essential cmake extra-cmake-modules ninja-build \
  qt6-base-dev qt6-declarative-dev libkf6windowsystem-dev
```

On **Fedora 39+:**
```bash
sudo dnf install \
  cmake extra-cmake-modules ninja-build \
  qt6-qtbase-devel qt6-qtdeclarative-devel kf6-kwindowsystem-devel
```

On **Arch Linux:**
```bash
sudo pacman -S cmake extra-cmake-modules ninja qt6-base qt6-declarative kwindowsystem
```

Then build and install:
```bash
git clone https://github.com/antroids/application-title-bar.git
cd application-title-bar
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
systemctl --user restart plasma-plasmashell.service
```

#### Enabling the app menu bar

After installation, add the element to the widget and enable the feature:

1. Right-click the widget → **Configure Application Title Bar**
2. In the **Appearance** tab, click **Add element...** and choose
   **Application Menu Bar** — drag it next to the Window Title element.
3. Check **Show app menu on hover** in the same tab.
4. Click **OK**.

---

### Additional packages

- Debian, Ubuntu, Kali Linux, Raspbian: `apt-get install qdbus`
- Alpine: `apk add qt5-qttools`
- Arch Linux: `pacman -S qdbus-qt5`
- CentOS: `yum install qdbus-qt5`
- Fedora: `dnf install qt5-qttools`

---

## Uninstalling

### 1. Remove the widget from your panel

Right-click the widget on the panel → **Remove**.

### 2. Uninstall the Plasma package

```bash
kpackagetool6 --type Plasma/Applet --remove com.github.antroids.application-title-bar
systemctl --user restart plasma-plasmashell.service
```

### 3. Remove the C++ plugin (only if you installed Option B)

Find and delete the plugin files that were installed into the Qt6 QML path:

```bash
# Locate the Qt6 QML directory on your system
QML_DIR=$(qmake6 -query QT_INSTALL_QML 2>/dev/null || echo "/usr/lib/qt6/qml")
sudo rm -rf "$QML_DIR/org/kde/applicationtitlebar"
```

If you installed via `cmake --install`, you can also use CMake's uninstall
target (if you still have the build directory):

```bash
cd application-title-bar
sudo cmake --build build --target uninstall
```

## 🆘 In cases of panel freezes or crashes 🆘

Although the widget is being used by me and a lot of other people, there is still a chance that it would be incompatible with your OS distribution. The worst that can happen is some Binding loop that can freeze your Plasma panel.

In such cases you can use the following script to downgrade the panel version:
`
wget https://github.com/antroids/application-title-bar/releases/download/v0.6.8/application-title-bar.plasmoid -O ${TMPDIR:-/tmp}/application-title-bar.plasmoid && kpackagetool6 -t Plasma/Applet -u ${TMPDIR:-/tmp}/application-title-bar.plasmoid && systemctl --user restart plasma-plasmashell.service
`

Or you can remove the widget: `kpackagetool6 --type Plasma/Applet --remove com.github.antroids.application-title-bar`

Please, don't forget to fill the report about the issues.

## License

This project is licensed under the GPL-3.0-or-later License - see the LICENSE.md file for details

## Contributing

Pull requests and Issue reports are always welcome.
