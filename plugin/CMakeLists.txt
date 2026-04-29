# SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(applicationtitlebar SHARED)

qt_add_qml_module(applicationtitlebar
    URI org.kde.applicationtitlebar
    VERSION 1.0
    SOURCES
        appmenumodel.h
        appmenumodel.cpp
)

target_link_libraries(applicationtitlebar PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Quick
    dbusmenu-qt6
)

# Install both the backing library and the plugin shared object
install(TARGETS applicationtitlebar applicationtitlebarplugin
    LIBRARY DESTINATION "${KDE_INSTALL_QMLDIR}/org/kde/applicationtitlebar"
    RUNTIME DESTINATION "${KDE_INSTALL_QMLDIR}/org/kde/applicationtitlebar"
)

# Install the generated qmldir so the QML engine can locate the module
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/qmldir"
    DESTINATION "${KDE_INSTALL_QMLDIR}/org/kde/applicationtitlebar"
)
