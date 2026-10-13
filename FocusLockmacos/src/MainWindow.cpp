#include "MainWindow.h"

#include "OverlayDialog.h"
#include "ProcessMonitor.h"
#include "Theme.h"
#include "WatchCard.h"
#include "MacUtil.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QVBoxLayout>

namespace {

constexpr int kDefaultMinutes = 30;

QLabel* addStatTile(QHBoxLayout* row, const QString& caption)
{
    auto* tile = new QFrame;
    tile->setObjectName("stat");
    auto* value = new QLabel("0");
    value->setObjectName("statValue");
    auto* label = new QLabel(caption);
    label->setObjectName("statCaption");
    auto* layout = new QVBoxLayout(tile);
    layout->setContentsMargins(18, 14, 18, 14);
    layout->setSpacing(2);
    layout->addWidget(value);
    layout->addWidget(label);
    row->addWidget(tile, 1);
    return value;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("FocusLock");
    resize(780, 640);
    setMinimumSize(580, 440);

    buildUi();
    setupTray();

    m_monitor = new ProcessMonitor;
    m_monitor->moveToThread(&m_monitorThread);
    connect(&m_monitorThread, &QThread::started, m_monitor, &ProcessMonitor::start);
    connect(&m_monitorThread, &QThread::finished, m_monitor, &QObject::deleteLater);
    connect(m_monitor, &ProcessMonitor::statusUpdated, this, &MainWindow::onStatus);
    connect(m_monitor, &ProcessMonitor::timeUp, this, &MainWindow::onTimeUp);
    connect(m_monitor, &ProcessMonitor::unlocked, this, &MainWindow::onUnlocked);
    m_monitorThread.setObjectName("ProcessMonitor");
    m_monitorThread.start();

    loadSettings();
    pushWatchlist();
}

MainWindow::~MainWindow()
{
    for (const auto& overlay : std::as_const(m_overlays))
        if (overlay)
            overlay->release();
    m_monitorThread.quit();
    m_monitorThread.wait();
}

void MainWindow::buildUi()
{
    auto* root = new QWidget;
    root->setObjectName("root");
    setCentralWidget(root);

    // Header
    auto* logo = new QLabel;
    logo->setPixmap(theme::appIcon().pixmap(40, 40));

    auto* title = new QLabel("FocusLock");
    title->setObjectName("title");
    auto* subtitle = new QLabel(
        "Give each app a time budget. When it runs out, the app stays locked until you close it.");
    subtitle->setObjectName("subtitle");
    subtitle->setWordWrap(true);

    auto* titles = new QVBoxLayout;
    titles->setSpacing(2);
    titles->addWidget(title);
    titles->addWidget(subtitle);

    auto* addButton = new QPushButton("+  Add app");
    addButton->setObjectName("primary");
    addButton->setCursor(Qt::PointingHandCursor);
    connect(addButton, &QPushButton::clicked, this, &MainWindow::pickApp);

    auto* header = new QHBoxLayout;
    header->setSpacing(14);
    header->addWidget(logo, 0, Qt::AlignTop);
    header->addLayout(titles, 1);
    header->addWidget(addButton, 0, Qt::AlignVCenter);

    // Stats
    auto* stats = new QHBoxLayout;
    stats->setSpacing(12);
    m_statWatching = addStatTile(stats, "Watching");
    m_statActive = addStatTile(stats, "Active now");
    m_statLocked = addStatTile(stats, "Locked");

    auto* section = new QLabel("WATCHLIST");
    section->setObjectName("section");

    // Empty state
    m_empty = new QFrame;
    m_empty->setObjectName("emptyState");
    auto* emptyTitle = new QLabel("Nothing on the watchlist yet");
    emptyTitle->setObjectName("emptyTitle");
    emptyTitle->setAlignment(Qt::AlignCenter);
    auto* emptyBody = new QLabel("Add an app from /Applications and pick how long a session may last.");
    emptyBody->setObjectName("emptyBody");
    emptyBody->setAlignment(Qt::AlignCenter);
    auto* emptyLayout = new QVBoxLayout(m_empty);
    emptyLayout->setContentsMargins(24, 40, 24, 40);
    emptyLayout->setSpacing(6);
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyBody);

    // List
    auto* host = new QWidget;
    host->setObjectName("listHost");
    m_list = new QVBoxLayout(host);
    m_list->setContentsMargins(0, 0, 6, 0);
    m_list->setSpacing(10);
    m_list->addWidget(m_empty);
    m_list->addStretch(1);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(host);

    auto* footer = new QLabel("Budgets drain only while an app has a window open, and refill 24 h after they start");
    footer->setObjectName("footer");

    auto* outer = new QVBoxLayout(root);
    outer->setContentsMargins(28, 24, 22, 18);
    outer->setSpacing(16);
    outer->addLayout(header);
    outer->addLayout(stats);
    outer->addSpacing(4);
    outer->addWidget(section);
    outer->addWidget(scroll, 1);
    outer->addWidget(footer);

    // Don't start with a caret blinking in the first card's budget field.
    root->setFocusPolicy(Qt::ClickFocus);
    root->setFocus();
}

void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    auto* menu = new QMenu(this);
    menu->addAction("Open FocusLock", this, &MainWindow::showFromTray);
    menu->addSeparator();
    menu->addAction("Quit", this, [this] {
        m_quitting = true;
        qApp->quit();
    });

    m_tray = new QSystemTrayIcon(theme::appIcon(), this);
    m_tray->setToolTip("FocusLock");
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            showFromTray();
    });
    m_tray->show();
}

void MainWindow::showFromTray()
{
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_quitting || !m_tray) {
        event->accept();
        qApp->quit();
        return;
    }

    // Keep timing in the background; quitting is in the tray menu.
    hide();
    event->ignore();
    if (!m_trayHintShown) {
        m_trayHintShown = true;
        m_tray->showMessage("FocusLock is still running",
                            "Timers keep counting in the background. Quit from the FocusLock icon in the menu bar.",
                            theme::appIcon(), 4000);
    }
}

void MainWindow::pickApp()
{
    const QString path = QFileDialog::getOpenFileName(this, "Choose an app to watch",
                                                      "/Applications", "Applications (*.app)");
    if (path.isEmpty())
        return;

    if (!addItem(path, kDefaultMinutes)) {
        QMessageBox::information(this, "Already watching",
                                 QString("%1 is already on the watchlist.").arg(QFileInfo(path).fileName()));
        return;
    }
    pushWatchlist();
}

bool MainWindow::addItem(const QString& path, int minutes)
{
    const QFileInfo info(path);
    // Processes are matched by the executable inside the bundle, which can differ
    // from the bundle's name (e.g. "Visual Studio Code.app" runs "Electron").
    QString exeName = macutil::bundleExecutable(info.absoluteFilePath());
    if (exeName.isEmpty())
        exeName = info.completeBaseName();
    const QString key = exeKey(exeName);
    if (key.isEmpty() || m_cards.contains(key))
        return false;

    WatchItem item;
    item.path = QDir::toNativeSeparators(info.absoluteFilePath());
    item.exeName = exeName;
    item.limitSecs = std::max(1, minutes) * 60;
    m_items.append(item);

    QString name = macutil::bundleDisplayName(item.path);
    if (name.isEmpty())
        name = info.completeBaseName();
    const QIcon icon = info.exists() ? QFileIconProvider().icon(info) : theme::appIcon();

    auto* card = new WatchCard(item, name, icon);
    connect(card, &WatchCard::limitChanged, this, [this, key](int m) { changeLimit(key, m); });
    connect(card, &WatchCard::removeRequested, this, [this, key] { removeItem(key); });
    m_list->insertWidget(m_list->count() - 1, card);
    m_cards.insert(key, card);

    refreshEmptyState();
    return true;
}

void MainWindow::removeItem(const QString& key)
{
    if (m_frozen.contains(key))
        return; // no escaping a running budget window by removing the app
    m_items.removeIf([&](const WatchItem& item) { return exeKey(item.exeName) == key; });
    if (WatchCard* card = m_cards.take(key))
        card->deleteLater();
    if (QPointer<OverlayDialog> overlay = m_overlays.take(key))
        overlay->release();

    refreshEmptyState();
    pushWatchlist();
}

void MainWindow::changeLimit(const QString& key, int minutes)
{
    if (m_frozen.contains(key))
        return; // the monitor ignores it anyway; the card snaps back on the next tick
    for (WatchItem& item : m_items) {
        if (exeKey(item.exeName) == key) {
            item.limitSecs = minutes * 60;
            break;
        }
    }
    pushWatchlist();
}

void MainWindow::pushWatchlist()
{
    saveSettings();
    QMetaObject::invokeMethod(
        m_monitor, [monitor = m_monitor, items = m_items] { monitor->setWatchlist(items); },
        Qt::QueuedConnection);
}

void MainWindow::loadSettings()
{
    QSettings settings;
    const int count = settings.beginReadArray("watchlist");
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        addItem(settings.value("path").toString(), settings.value("minutes", kDefaultMinutes).toInt());
    }
    settings.endArray();
}

void MainWindow::saveSettings() const
{
    QSettings settings;
    settings.beginWriteArray("watchlist", int(m_items.size()));
    for (int i = 0; i < m_items.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("path", m_items[i].path);
        settings.setValue("minutes", m_items[i].limitSecs / 60);
    }
    settings.endArray();
}

void MainWindow::refreshEmptyState()
{
    m_empty->setVisible(m_items.isEmpty());
    m_statWatching->setText(QString::number(m_items.size()));
}

void MainWindow::onStatus(const QList<WatchStatus>& statuses)
{
    int active = 0, locked = 0;
    bool limitsSynced = false;
    for (const WatchStatus& status : statuses) {
        const QString key = exeKey(status.exeName);
        WatchCard* card = m_cards.value(key);
        if (!card)
            continue; // removed since this tick started

        // While a budget window runs, the monitor's frozen limit wins (e.g. after the
        // app was re-added with the default limit).
        if (status.refillInSecs > 0) {
            m_frozen.insert(key);
            for (WatchItem& item : m_items) {
                if (exeKey(item.exeName) == key && item.limitSecs != status.limitSecs) {
                    item.limitSecs = status.limitSecs;
                    limitsSynced = true;
                }
            }
        } else {
            m_frozen.remove(key);
        }

        card->setStatus(status);
        active += status.state == WatchState::Active;
        locked += status.state == WatchState::Locked;

        if (status.state == WatchState::Locked)
            if (OverlayDialog* overlay = m_overlays.value(key))
                overlay->setPids(status.pids);
    }
    m_statActive->setText(QString::number(active));
    m_statLocked->setText(QString::number(locked));
    if (limitsSynced)
        saveSettings();
}

void MainWindow::onTimeUp(const QString& exeName, const QList<quint32>& pids, qint64 refillInSecs)
{
    const QString key = exeKey(exeName);
    WatchCard* card = m_cards.value(key);
    if (!card || m_overlays.value(key))
        return;

    int limitSecs = 0;
    for (const WatchItem& item : std::as_const(m_items))
        if (exeKey(item.exeName) == key)
            limitSecs = item.limitSecs;

    auto* overlay = new OverlayDialog(card->displayName(), card->icon(), limitSecs, refillInSecs);
    overlay->setPids(pids);
    m_overlays.insert(key, overlay);
    overlay->show();
    overlay->raise();
    overlay->activateWindow();
}

void MainWindow::onUnlocked(const QString& exeName)
{
    if (QPointer<OverlayDialog> overlay = m_overlays.take(exeKey(exeName)))
        overlay->release();
}
