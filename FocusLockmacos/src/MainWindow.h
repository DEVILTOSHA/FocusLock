#pragma once

#include "WatchTypes.h"

#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QSet>
#include <QThread>

class OverlayDialog;
class ProcessMonitor;
class QLabel;
class QSystemTrayIcon;
class QVBoxLayout;
class WatchCard;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void setupTray();
    void showFromTray();

    void pickApp();
    bool addItem(const QString& path, int minutes);
    void removeItem(const QString& key);
    void changeLimit(const QString& key, int minutes);
    void pushWatchlist();
    void loadSettings();
    void saveSettings() const;
    void refreshEmptyState();

    void onStatus(const QList<WatchStatus>& statuses);
    void onTimeUp(const QString& exeName, const QList<quint32>& pids, qint64 refillInSecs);
    void onUnlocked(const QString& exeName);

    QList<WatchItem> m_items;
    QHash<QString, WatchCard*> m_cards;                  // keyed by exeKey()
    QHash<QString, QPointer<OverlayDialog>> m_overlays;  // keyed by exeKey()
    QSet<QString> m_frozen;                              // apps whose limit is locked

    QThread m_monitorThread;
    ProcessMonitor* m_monitor = nullptr;

    QVBoxLayout* m_list = nullptr;
    QWidget* m_empty = nullptr;
    QLabel* m_statWatching = nullptr;
    QLabel* m_statActive = nullptr;
    QLabel* m_statLocked = nullptr;

    QSystemTrayIcon* m_tray = nullptr;
    bool m_quitting = false;
    bool m_trayHintShown = false;
};
