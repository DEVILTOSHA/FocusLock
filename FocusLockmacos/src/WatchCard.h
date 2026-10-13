#pragma once

#include "WatchTypes.h"

#include <QFrame>
#include <QIcon>

class QLabel;
class QProgressBar;
class QSpinBox;
class QToolButton;

// One row of the watchlist: app identity, live countdown, and budget controls.
class WatchCard : public QFrame {
    Q_OBJECT

public:
    WatchCard(const WatchItem& item, const QString& displayName, const QIcon& icon,
              QWidget* parent = nullptr);

    QString displayName() const { return m_displayName; }
    QIcon icon() const { return m_icon; }

    void setStatus(const WatchStatus& status);

private:
    void setFrozen(bool frozen, qint64 refillInSecs);

signals:
    void limitChanged(int minutes);
    void removeRequested();

private:
    QString m_displayName;
    QIcon m_icon;
    QString m_state;
    QLabel* m_pill = nullptr;
    QLabel* m_time = nullptr;
    QLabel* m_refill = nullptr;
    QProgressBar* m_bar = nullptr;
    QSpinBox* m_minutes = nullptr;
    QToolButton* m_minus = nullptr;
    QToolButton* m_plus = nullptr;
    QToolButton* m_remove = nullptr;
};
