#pragma once

#include <QDialog>
#include <QIcon>
#include <QTimer>

class QFrame;
class QLabel;
class QPushButton;

// Frameless, always-on-top lock screen for one app whose budget ran out.
//
// While the target window is visible the overlay covers it exactly (tracked via
// GetWindowRect every 200 ms) with a dimmed backdrop and a centred card. When the
// target is minimised, or while it is being asked to close, it shrinks to just
// the card so the app's own "save changes?" prompt stays reachable.
//
// The user can't dismiss it: Esc and Alt+F4 are ignored. Only release() closes it,
// which MainWindow calls once ProcessMonitor sees the target exit or the budget refill.
class OverlayDialog : public QDialog {
    Q_OBJECT

public:
    OverlayDialog(const QString& appName, const QIcon& icon, int limitSecs, qint64 refillInSecs,
                  QWidget* parent = nullptr);

    void setPids(const QList<quint32>& pids);
    void release();

public slots:
    void reject() override;

protected:
    void showEvent(QShowEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void reposition();
    void requestTargetClose();
    void setCompact(bool compact);

    QString m_appName;
    QList<quint32> m_pids;
    QTimer m_follow;
    QFrame* m_card = nullptr;
    QPushButton* m_closeButton = nullptr;
    QLabel* m_hint = nullptr;
    bool m_released = false;
    bool m_compact = false;
    bool m_closing = false;
};
