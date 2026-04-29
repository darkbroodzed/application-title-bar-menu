# SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)
project(application-title-bar)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Quick)
find_package(dbusmenu-qt6 REQUIRED)

add_subdirectory(plugin)

# Install the Plasma QML package directly to the plasmoids data directory.
# This is equivalent to plasma_install_package() but avoids a hard dependency
# on the Plasma CMake config (which is not available on every build host).
install(DIRECTORY package/
    DESTINATION "${KDE_INSTALL_DATADIR}/plasma/plasmoids/com.github.antroids.application-title-bar"
)
