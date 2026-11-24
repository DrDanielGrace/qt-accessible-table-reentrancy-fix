// Minimal reproducer for the Qt 6.12.0 QAccessibleCache re-entrancy bug.
//
// The test deliberately creates the same shape of callback that Cocoa
// accessibility can perform synchronously while Qt is sending an
// ObjectDestroyed event:
//
//     QAccessible::deleteAccessibleInterface(id)
//       -> QAccessibleCache::deleteInterface(id)
//         -> QAccessible::updateAccessibility(ObjectDestroyed)
//           -> Cocoa callback
//             -> QAccessible::deleteAccessibleInterface(id)   [re-entry]
//
// With Qt 6.12.0, the outer deleteInterface() keeps an iterator into
// idToInterface across updateAccessibility(). The nested call erases and
// deletes the interface, and the outer call then uses the stale iterator and
// deletes the interface a second time.
//
// Expected result:
//   Qt 6.12.0 (unpatched): crash / ASan heap-use-after-free / double delete.
//   Qt with QTBUG-149629 fix: prints PASS and exits 0.

#include <QAccessible>
#include <QAccessibleObject>
#include <QApplication>
#include <QDebug>
#include <QTimer>

namespace {

QAccessible::Id g_targetId = 0;
bool g_reentered = false;
int g_destructorCount = 0;

// An interface with object() == nullptr. QAccessibleTable's cell and header
// interfaces have exactly this property, which is why deleting them sends an
// ObjectDestroyed event from QAccessibleCache::deleteInterface().
class FictionalCellInterface final : public QAccessibleObject
{
public:
    FictionalCellInterface()
        : QAccessibleObject(nullptr)
    {
    }

    ~FictionalCellInterface() override
    {
        ++g_destructorCount;
        qInfo() << "FictionalCellInterface destructor, count =" << g_destructorCount;
    }

    QAccessible::Role role() const override { return QAccessible::Cell; }
    QAccessible::State state() const override { return QAccessible::State(); }
    QString text(QAccessible::Text) const override
    {
        return QStringLiteral("fictional cell A1");
    }
    int childCount() const override { return 0; }
    QAccessibleInterface *child(int) const override { return nullptr; }
    QAccessibleInterface *parent() const override { return nullptr; }
    int indexOfChild(const QAccessibleInterface *) const override { return -1; }
};

void onAccessibilityUpdate(QAccessibleEvent *event)
{
    if (event->type() != QAccessible::ObjectDestroyed
        || event->uniqueId() != g_targetId) {
        return;
    }

    qInfo() << "update handler: ObjectDestroyed for id" << event->uniqueId();

    // Simulate the platform callback that re-enters the cache. The guard only
    // prevents infinite recursion in this artificial callback; the first
    // nested deletion is enough to corrupt the outer deletion.
    if (g_reentered)
        return;
    g_reentered = true;

    qInfo() << "update handler: re-entering deleteAccessibleInterface";
    QAccessible::deleteAccessibleInterface(g_targetId);
    qInfo() << "update handler: nested deleteAccessibleInterface returned";
}

} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("FictionalAccessibleCacheRepro"));

    qInfo().noquote() << "Qt runtime version:" << qVersion();

    // Make QAccessible::isActive() true without changing any macOS setting or
    // starting VoiceOver. deleteInterface() only sends ObjectDestroyed when
    // accessibility is active.
    QAccessible::setActive(true);
    if (!QAccessible::isActive()) {
        qCritical() << "Could not activate the Qt accessibility layer";
        return 2;
    }

    auto *iface = new FictionalCellInterface;
    g_targetId = QAccessible::registerAccessibleInterface(iface);
    qInfo() << "registered objectless interface with id" << g_targetId;

    const auto previousHandler = QAccessible::installUpdateHandler(&onAccessibilityUpdate);

    QTimer::singleShot(0, &app, [&] {
        // QGuiApplicationPrivate::is_app_running is now true, so the
        // ObjectDestroyed event is sent.
        qInfo() << "outer deleteAccessibleInterface begins";
        QAccessible::deleteAccessibleInterface(g_targetId);
        qInfo() << "outer deleteAccessibleInterface returned";

        QAccessible::installUpdateHandler(previousHandler);

        const bool stillCached = QAccessible::accessibleInterface(g_targetId) != nullptr;
        qInfo() << "interface still in cache:" << stillCached;
        qInfo() << "interface destructor count:" << g_destructorCount;

        if (stillCached || g_destructorCount != 1) {
            qCritical() << "FAIL: inconsistent cache/destructor state";
            app.exit(1);
            return;
        }

        qInfo() << "PASS: nested deletion was handled without touching a stale iterator";
        app.exit(0);
    });

    return app.exec();
}
