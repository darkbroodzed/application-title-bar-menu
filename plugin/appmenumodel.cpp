/*
 * SPDX-FileCopyrightText: 2024 Anton Kharuzhy <publicantroids@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "appmenumodel.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QMenu>
#include <QPoint>
#include <QQuickItem>
#include <QQuickWindow>
#include <QRect>
#include <QScreen>

// ── Internal helpers ─────────────────────────────────────────────────────────

// Strip underscore-based mnemonic markers used by the dbusmenu protocol
// (e.g. "_File" → "File").
static QString cleanLabel(const QString &raw)
{
    QString out;
    out.reserve(raw.size());
    bool skipNext = false;
    for (const QChar ch : raw) {
        if (skipNext) {
            out += ch;
            skipNext = false;
        } else if (ch == QLatin1Char('_')) {
            skipNext = true;   // next char is the mnemonic letter — keep it
        } else {
            out += ch;
        }
    }
    return out;
}

// Recursive representation of one node in a com.canonical.dbusmenu layout.
struct DbusMenuItem {
    int          id = 0;
    QVariantMap  properties;
    QList<DbusMenuItem> children;
};

// Parse a layout item of D-Bus signature (ia{sv}av) from a QDBusArgument.
// The argument must be positioned at the start of the structure.
static DbusMenuItem parseItem(const QDBusArgument &arg)
{
    DbusMenuItem item;
    arg.beginStructure();
    arg >> item.id;
    arg >> item.properties;
    arg.beginArray();
    while (!arg.atEnd()) {
        QVariant childVar;
        arg >> childVar;
        // Children arrive as variants wrapping another (ia{sv}av) struct.
        if (childVar.userType() == qMetaTypeId<QDBusArgument>()) {
            item.children.append(parseItem(qvariant_cast<QDBusArgument>(childVar)));
        }
    }
    arg.endArray();
    arg.endStructure();
    return item;
}

// ── Static member ─────────────────────────────────────────────────────────────

int AppMenuModel::s_instanceCount = 0;

// ── Construction / destruction ───────────────────────────────────────────────

AppMenuModel::AppMenuModel(QObject *parent)
    : QAbstractListModel(parent)
    , m_serviceWatcher(new QDBusServiceWatcher(this))
{
    // Registering this well-known name signals GTK / KDE applications that a
    // global-menu consumer is present so they start exporting via dbusmenu.
    if (++s_instanceCount == 1) {
        QDBusConnection::sessionBus().registerService(
            QStringLiteral("org.kde.kappmenuview"));
    }

    m_serviceWatcher->setConnection(QDBusConnection::sessionBus());
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered,
            this, [this](const QString &) {
        beginResetModel();
        m_entries.clear();
        endResetModel();
        setMenuAvailable(false);
    });
}

AppMenuModel::~AppMenuModel()
{
    if (--s_instanceCount == 0) {
        QDBusConnection::sessionBus().unregisterService(
            QStringLiteral("org.kde.kappmenuview"));
    }
}

// ── QAbstractListModel ───────────────────────────────────────────────────────

QVariant AppMenuModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.count())
        return {};

    const MenuEntry &e = m_entries.at(index.row());
    switch (role) {
    case MenuRole:
        return e.label;
    case ActionRole:
        // Return a QVariantMap so QML can access activeActions.enabled
        return QVariantMap{{QStringLiteral("enabled"), e.enabled}};
    }
    return {};
}

int AppMenuModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.count();
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
    if (index < 0 || index >= m_entries.count())
        return;

    const int itemId = m_entries.at(index).id;

    // Ask the application to prepare the submenu before we fetch it.
    QDBusInterface iface(m_serviceName, m_menuObjectPath,
                         QStringLiteral("com.canonical.dbusmenu"),
                         QDBusConnection::sessionBus());
    iface.call(QDBus::NoBlock, QStringLiteral("AboutToShow"), itemId);

    QMenu *menu = buildSubmenu(itemId);
    if (!menu || menu->isEmpty()) {
        delete menu;
        return;
    }
    menu->setAttribute(Qt::WA_DeleteOnClose);

    QPoint globalPos;
    if (ctx && ctx->window()) {
        const QPointF below = ctx->mapToScene(QPointF(0.0, ctx->height()));
        globalPos = ctx->window()->mapToGlobal(below.toPoint());

        if (ctx->window()->screen()) {
            const QRect avail = ctx->window()->screen()->availableGeometry();
            if (globalPos.y() + menu->sizeHint().height() > avail.bottom()) {
                const QPointF above = ctx->mapToScene(QPointF(0.0, 0.0));
                const QPoint topGlobal = ctx->window()->mapToGlobal(above.toPoint());
                globalPos = QPoint(topGlobal.x(),
                                   topGlobal.y() - menu->sizeHint().height());
            }
        }
    }

    menu->popup(globalPos);
}

// ── Property accessors ───────────────────────────────────────────────────────

QString AppMenuModel::serviceName() const { return m_serviceName; }

void AppMenuModel::setServiceName(const QString &name)
{
    if (m_serviceName == name) return;
    m_serviceName = name;
    Q_EMIT serviceNameChanged();
    m_serviceWatcher->setWatchedServices({name});
    updateMenu();
}

QString AppMenuModel::menuObjectPath() const { return m_menuObjectPath; }

void AppMenuModel::setMenuObjectPath(const QString &path)
{
    if (m_menuObjectPath == path) return;
    m_menuObjectPath = path;
    Q_EMIT menuObjectPathChanged();
    updateMenu();
}

bool AppMenuModel::menuAvailable() const { return m_menuAvailable; }

// ── Private helpers ──────────────────────────────────────────────────────────

void AppMenuModel::setMenuAvailable(bool available)
{
    if (m_menuAvailable == available) return;
    m_menuAvailable = available;
    Q_EMIT menuAvailableChanged();
}

void AppMenuModel::updateMenu()
{
    beginResetModel();
    m_entries.clear();
    endResetModel();

    if (m_serviceName.isEmpty() || m_menuObjectPath.isEmpty()) {
        setMenuAvailable(false);
        return;
    }

    QDBusInterface iface(m_serviceName, m_menuObjectPath,
                         QStringLiteral("com.canonical.dbusmenu"),
                         QDBusConnection::sessionBus());
    if (!iface.isValid()) {
        setMenuAvailable(false);
        return;
    }

    // Fetch only the immediate children of the root (recursionDepth = 1).
    const QDBusMessage reply = iface.call(
        QStringLiteral("GetLayout"),
        0,      // parentId  = root
        1,      // recursionDepth
        QStringList{QStringLiteral("label"),
                    QStringLiteral("enabled"),
                    QStringLiteral("visible"),
                    QStringLiteral("type")});

    if (reply.type() != QDBusMessage::ReplyMessage
            || reply.arguments().size() < 2) {
        setMenuAvailable(false);
        return;
    }

    // Reply: (uint32 revision, (int32 id, {sv} props, av children))
    const QDBusArgument layoutArg =
        reply.arguments().at(1).value<QDBusArgument>();
    const DbusMenuItem root = parseItem(layoutArg);

    beginResetModel();
    for (const DbusMenuItem &child : root.children) {
        if (child.properties.value(QStringLiteral("type")).toString()
                == QStringLiteral("separator"))
            continue;
        if (!child.properties.value(QStringLiteral("visible"), true).toBool())
            continue;
        const QString label =
            cleanLabel(child.properties.value(QStringLiteral("label")).toString());
        if (label.isEmpty())
            continue;
        m_entries.append({
            child.id,
            label,
            child.properties.value(QStringLiteral("enabled"), true).toBool()
        });
    }
    endResetModel();
    setMenuAvailable(!m_entries.isEmpty());
}

// Build a native QMenu by fetching all descendants of parentId.
QMenu *AppMenuModel::buildSubmenu(int parentId)
{
    QDBusInterface iface(m_serviceName, m_menuObjectPath,
                         QStringLiteral("com.canonical.dbusmenu"),
                         QDBusConnection::sessionBus());

    const QDBusMessage reply = iface.call(
        QStringLiteral("GetLayout"),
        parentId,
        -1,     // all descendants
        QStringList{QStringLiteral("label"),
                    QStringLiteral("enabled"),
                    QStringLiteral("visible"),
                    QStringLiteral("type"),
                    QStringLiteral("children-display")});

    if (reply.type() != QDBusMessage::ReplyMessage
            || reply.arguments().size() < 2)
        return nullptr;

    const QDBusArgument layoutArg =
        reply.arguments().at(1).value<QDBusArgument>();
    const DbusMenuItem root = parseItem(layoutArg);

    // Recursive lambda to turn a DbusMenuItem tree into a QMenu tree.
    std::function<void(QMenu *, const QList<DbusMenuItem> &)> populate;
    populate = [&](QMenu *menu, const QList<DbusMenuItem> &items) {
        for (const DbusMenuItem &item : items) {
            const QString type =
                item.properties.value(QStringLiteral("type")).toString();

            if (type == QStringLiteral("separator")) {
                menu->addSeparator();
                continue;
            }
            if (!item.properties.value(QStringLiteral("visible"), true).toBool())
                continue;

            const QString label =
                cleanLabel(item.properties.value(QStringLiteral("label")).toString());
            const bool enabled =
                item.properties.value(QStringLiteral("enabled"), true).toBool();
            const bool hasChildren =
                item.properties.value(QStringLiteral("children-display")).toString()
                == QStringLiteral("submenu")
                || !item.children.isEmpty();

            if (hasChildren) {
                QMenu *sub = menu->addMenu(label);
                sub->setEnabled(enabled);
                populate(sub, item.children);
            } else {
                const int itemId = item.id;
                QAction *action = menu->addAction(label);
                action->setEnabled(enabled);
                connect(action, &QAction::triggered, this,
                        [this, itemId] { sendEvent(itemId, QStringLiteral("clicked")); });
            }
        }
    };
    auto *menu = new QMenu;
    populate(menu, root.children);
    return menu;
}

void AppMenuModel::sendEvent(int id, const QString &eventType)
{
    QDBusInterface iface(m_serviceName, m_menuObjectPath,
                         QStringLiteral("com.canonical.dbusmenu"),
                         QDBusConnection::sessionBus());
    iface.call(QDBus::NoBlock, QStringLiteral("Event"),
               id, eventType, QVariant(0),
               static_cast<uint>(QDateTime::currentSecsSinceEpoch()));
}
