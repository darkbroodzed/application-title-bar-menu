/*
 * SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "appmenumodel.h"

#include <QAction>
#include <QDBusConnection>
#include <QMenu>
#include <QPoint>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>

#include <dbusmenuimporter.h>

// ── Static initialisation ────────────────────────────────────────────────────

int AppMenuModel::s_instanceCount = 0;

// ── Construction / destruction ───────────────────────────────────────────────

AppMenuModel::AppMenuModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
{
    // Register the well-known service name the first time an AppMenuModel is
    // created.  Applications (GTK, KDE, …) watch for this name and start
    // exporting their menus via D-Bus when they see it appear.
    if (++s_instanceCount == 1) {
        QDBusConnection::sessionBus().registerService(QStringLiteral("org.kde.kappmenuview"));
    }

    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    // If the application's D-Bus service disappears (e.g. the app closed),
    // clear the menu so stale entries are not shown.
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, this, [this](const QString &) {
        beginResetModel();
        m_menu.clear();
        m_importer.reset();
        endResetModel();
        setMenuAvailable(false);
    });
}

AppMenuModel::~AppMenuModel()
{
    if (--s_instanceCount == 0) {
        QDBusConnection::sessionBus().unregisterService(QStringLiteral("org.kde.kappmenuview"));
    }
}

// ── QAbstractListModel ───────────────────────────────────────────────────────

QVariant AppMenuModel::data(const QModelIndex &index, int role) const
{
    if (!m_menu || !index.isValid() || index.row() >= m_menu->actions().count()) {
        return QVariant();
    }

    QAction *action = m_menu->actions().at(index.row());

    switch (role) {
    case MenuRole:
        return action->text();
    case ActionRole:
        // Return as QObject* so QML can access Q_PROPERTY members (e.g. .enabled)
        return QVariant::fromValue(static_cast<QObject *>(action));
    }

    return QVariant();
}

int AppMenuModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid() || !m_menu) {
        return 0;
    }
    return m_menu->actions().count();
}

QHash<int, QByteArray> AppMenuModel::roleNames() const
{
    return {
        {MenuRole,   QByteArrayLiteral("activeMenu")},
        {ActionRole, QByteArrayLiteral("activeActions")},
    };
}

// ── Public invokable ─────────────────────────────────────────────────────────

void AppMenuModel::trigger(QQuickItem *ctx, int index)
{
    if (!m_menu || index < 0 || index >= m_menu->actions().count()) {
        return;
    }

    QAction *action = m_menu->actions().at(index);
    if (!action || !action->menu()) {
        return;
    }

    QMenu *menu = action->menu();

    // Default: open the popup just below the button.
    QPoint globalPos;
    if (ctx && ctx->window()) {
        const QPointF scenePosBelow = ctx->mapToScene(QPointF(0.0, ctx->height()));
        globalPos = ctx->window()->mapToGlobal(scenePosBelow.toPoint());

        // If the menu would extend below the available screen area, flip it
        // upward so it appears above the button instead (bottom-panel support).
        if (ctx->window()->screen()) {
            const QRect screenGeo = ctx->window()->screen()->availableGeometry();
            if (globalPos.y() + menu->sizeHint().height() > screenGeo.bottom()) {
                const QPointF scenePosAbove = ctx->mapToScene(QPointF(0.0, 0.0));
                const QPoint topGlobal = ctx->window()->mapToGlobal(scenePosAbove.toPoint());
                globalPos = QPoint(topGlobal.x(), topGlobal.y() - menu->sizeHint().height());
            }
        }
    }

    menu->popup(globalPos);
}

// ── Property accessors ───────────────────────────────────────────────────────

QString AppMenuModel::serviceName() const
{
    return m_serviceName;
}

void AppMenuModel::setServiceName(const QString &name)
{
    if (m_serviceName == name) {
        return;
    }
    m_serviceName = name;
    Q_EMIT serviceNameChanged();

    // Update the service watcher so we notice when this app's service disappears.
    m_serviceWatcher->setWatchedServices({name});
    updateMenu();
}

QString AppMenuModel::menuObjectPath() const
{
    return m_menuObjectPath;
}

void AppMenuModel::setMenuObjectPath(const QString &path)
{
    if (m_menuObjectPath == path) {
        return;
    }
    m_menuObjectPath = path;
    Q_EMIT menuObjectPathChanged();
    updateMenu();
}

bool AppMenuModel::menuAvailable() const
{
    return m_menuAvailable;
}

// ── Private helpers ──────────────────────────────────────────────────────────

void AppMenuModel::setMenuAvailable(bool available)
{
    if (m_menuAvailable == available) {
        return;
    }
    m_menuAvailable = available;
    Q_EMIT menuAvailableChanged();
}

void AppMenuModel::updateMenu()
{
    // Without both coordinates we have nothing to import.
    if (m_serviceName.isEmpty() || m_menuObjectPath.isEmpty()) {
        beginResetModel();
        m_menu.clear();
        m_importer.reset();
        endResetModel();
        setMenuAvailable(false);
        return;
    }

    // Destroy the previous importer before creating a new one so that
    // DBusMenuImporter does not try to reuse a stale connection.
    m_importer.reset();

    m_importer = std::make_unique<DBusMenuImporter>(m_serviceName, m_menuObjectPath, this);
    connect(m_importer.get(), &DBusMenuImporter::menuUpdated, this, &AppMenuModel::onMenuUpdated);

    // Trigger the initial fetch.
    m_importer->updateMenu();
}

void AppMenuModel::onMenuUpdated(QMenu *menu)
{
    beginResetModel();
    m_menu = menu;
    endResetModel();

    setMenuAvailable(menu != nullptr && !menu->actions().isEmpty());
}
