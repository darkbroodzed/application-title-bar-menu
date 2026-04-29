/*
 * SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QDBusServiceWatcher>
#include <QPointer>
#include <QQmlEngine>
#include <memory>

class DBusMenuImporter;
class QMenu;
class QQuickItem;

/**
 * AppMenuModel exposes the active window's D-Bus application menu as a flat
 * list of top-level entries (e.g. File, Edit, View …) that AppMenuBar.qml
 * renders as a row of ToolButtons.
 *
 * It is a simplified port of plasma-workspace's AppMenuModel.  The key
 * difference is that it has no internal TasksModel dependency: the D-Bus
 * service name and object path are fed as QML properties from the existing
 * ActiveTasksModel, keeping this class small and testable.
 *
 * Registering the well-known D-Bus service "org.kde.kappmenuview" (done in
 * the constructor / destructor using a reference-count) is what signals GTK
 * and KDE applications that a global menu consumer is present, causing them
 * to export their menus via the com.canonical.dbusmenu protocol.
 */
class AppMenuModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    // Fed from QML via tasksModel.activeWindow
    Q_PROPERTY(QString serviceName READ serviceName WRITE setServiceName NOTIFY serviceNameChanged)
    Q_PROPERTY(QString menuObjectPath READ menuObjectPath WRITE setMenuObjectPath NOTIFY menuObjectPathChanged)
    Q_PROPERTY(bool menuAvailable READ menuAvailable NOTIFY menuAvailableChanged)

public:
    enum Roles {
        /// Display text of the top-level menu entry (e.g. "&File")
        MenuRole = Qt::UserRole + 1,
        /// The QAction* for the top-level entry (QObject* in QML)
        ActionRole,
    };
    Q_ENUM(Roles)

    explicit AppMenuModel(QObject *parent = nullptr);
    ~AppMenuModel() override;

    // QAbstractListModel interface
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int rowCount(const QModelIndex &parent = {}) const override;
    QHash<int, QByteArray> roleNames() const override;

    /**
     * Invoked from QML when the user clicks a top-level menu button.
     * @param ctx  The ToolButton that was clicked (used to compute popup position).
     * @param index  Row index into this model.
     */
    Q_INVOKABLE void trigger(QQuickItem *ctx, int index);

    QString serviceName() const;
    void setServiceName(const QString &name);

    QString menuObjectPath() const;
    void setMenuObjectPath(const QString &path);

    bool menuAvailable() const;

Q_SIGNALS:
    void serviceNameChanged();
    void menuObjectPathChanged();
    void menuAvailableChanged();
    /// Emitted for keyboard left/right navigation between top-level entries.
    void requestActivateIndex(int index);

private:
    void updateMenu();
    void onMenuUpdated(QMenu *menu);
    void setMenuAvailable(bool available);

    QString m_serviceName;
    QString m_menuObjectPath;
    bool m_menuAvailable = false;

    std::unique_ptr<DBusMenuImporter> m_importer;
    QPointer<QMenu> m_menu;
    QDBusServiceWatcher *m_serviceWatcher = nullptr;

    // Reference count for the "org.kde.kappmenuview" D-Bus service name.
    // The service is registered on first AppMenuModel construction and
    // unregistered when the last instance is destroyed.
    static int s_instanceCount;
};
