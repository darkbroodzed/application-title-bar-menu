/*
 * SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <QAbstractListModel>
#include <QDBusServiceWatcher>
#include <QQmlEngine>

class QMenu;
class QQuickItem;
Q_DECLARE_OPAQUE_POINTER(QQuickItem *)

/**
 * AppMenuModel exposes the active window's D-Bus application menu
 * (com.canonical.dbusmenu protocol) as a flat list of top-level entries
 * rendered by AppMenuBar.qml as a row of ToolButtons.
 *
 * This implementation uses Qt6::DBus directly — no external dbusmenu-qt
 * library is required.
 */
class AppMenuModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QString serviceName    READ serviceName    WRITE setServiceName    NOTIFY serviceNameChanged)
    Q_PROPERTY(QString menuObjectPath READ menuObjectPath WRITE setMenuObjectPath NOTIFY menuObjectPathChanged)
    Q_PROPERTY(bool    menuAvailable  READ menuAvailable                          NOTIFY menuAvailableChanged)

public:
    // Internal representation of one top-level menu entry.
    struct MenuEntry {
        int     id      = 0;
        QString label;
        bool    enabled = true;
    };

    enum Roles {
        MenuRole   = Qt::UserRole + 1,  // QString  — display text
        ActionRole,                     // QVariantMap { "enabled": bool }
    };
    Q_ENUM(Roles)

    explicit AppMenuModel(QObject *parent = nullptr);
    ~AppMenuModel() override;

    QVariant           data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    int                rowCount(const QModelIndex &parent = {}) const override;
    QHash<int,QByteArray> roleNames() const override;

    Q_INVOKABLE void trigger(QQuickItem *ctx, int index);

    QString serviceName()    const;
    void    setServiceName(const QString &name);
    QString menuObjectPath() const;
    void    setMenuObjectPath(const QString &path);
    bool    menuAvailable()  const;

Q_SIGNALS:
    void serviceNameChanged();
    void menuObjectPathChanged();
    void menuAvailableChanged();
    void requestActivateIndex(int index);

private:
    void   updateMenu();
    void   setMenuAvailable(bool available);
    QMenu *buildSubmenu(int parentId);
    void   sendEvent(int id, const QString &eventType);

    QString  m_serviceName;
    QString  m_menuObjectPath;
    bool     m_menuAvailable = false;

    QList<MenuEntry>      m_entries;
    QDBusServiceWatcher  *m_serviceWatcher = nullptr;

    static int s_instanceCount;
};
