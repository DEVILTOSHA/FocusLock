#include "WatchCard.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

QToolButton* makeStepper(const QString& text, const QString& tip)
{
    auto* button = new QToolButton;
    button->setObjectName("stepper");
    button->setText(text);
    button->setToolTip(tip);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRepeat(true);
    return button;
}

void restyle(QWidget* widget, const QString& state)
{
    widget->setProperty("state", state);
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
}

} // namespace

WatchCard::WatchCard(const WatchItem& item, const QString& displayName, const QIcon& icon,
                     QWidget* parent)
    : QFrame(parent)
    , m_displayName(displayName)
    , m_icon(icon)
{
    setObjectName("card");

    auto* iconLabel = new QLabel;
    iconLabel->setObjectName("appIcon");
    iconLabel->setPixmap(icon.pixmap(28, 28));
    iconLabel->setFixedSize(44, 44);
    iconLabel->setAlignment(Qt::AlignCenter);

    auto* name = new QLabel(displayName);
    name->setObjectName("cardTitle");

    m_pill = new QLabel;
    m_pill->setObjectName("pill");
    m_pill->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    auto* nameRow = new QHBoxLayout;
    nameRow->setSpacing(8);
    nameRow->addWidget(name);
    nameRow->addWidget(m_pill);
    nameRow->addStretch();

    auto* path = new QLabel;
    path->setObjectName("cardPath");
    path->setToolTip(item.path);
    path->setText(path->fontMetrics().elidedText(item.path, Qt::ElideMiddle, 340));
    path->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);

    m_refill = new QLabel;
    m_refill->setObjectName("refill");
    m_refill->hide();

    m_bar = new QProgressBar;
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(4);

    auto* info = new QVBoxLayout;
    info->setSpacing(4);
    info->addLayout(nameRow);
    info->addWidget(path);
    info->addWidget(m_refill);
    info->addSpacing(6);
    info->addWidget(m_bar);

    m_time = new QLabel;
    m_time->setObjectName("time");
    m_time->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_minutes = new QSpinBox;
    m_minutes->setRange(1, 720);
    m_minutes->setValue(std::max(1, item.limitSecs / 60));
    m_minutes->setSuffix(" min");
    m_minutes->setAlignment(Qt::AlignCenter);
    m_minutes->setKeyboardTracking(false);
    m_minutes->setFixedWidth(76);
    m_minutes->setToolTip("Session budget");

    m_minus = makeStepper("−", "Less time");
    m_plus = makeStepper("+", "More time");
    connect(m_minus, &QToolButton::clicked, this, [this] {
        const int v = m_minutes->value();
        m_minutes->setValue(v > 5 ? v - 5 : v - 1);
    });
    connect(m_plus, &QToolButton::clicked, this, [this] {
        const int v = m_minutes->value();
        m_minutes->setValue(v >= 5 ? v + 5 : v + 1);
    });

    auto* budgetRow = new QHBoxLayout;
    budgetRow->setSpacing(4);
    budgetRow->addStretch();
    budgetRow->addWidget(m_minus);
    budgetRow->addWidget(m_minutes);
    budgetRow->addWidget(m_plus);

    auto* right = new QVBoxLayout;
    right->setSpacing(6);
    right->addWidget(m_time);
    right->addLayout(budgetRow);

    m_remove = new QToolButton;
    m_remove->setObjectName("remove");
    m_remove->setText("✕");
    m_remove->setCursor(Qt::PointingHandCursor);

    auto* row = new QHBoxLayout(this);
    row->setContentsMargins(16, 14, 10, 14);
    row->setSpacing(14);
    row->addWidget(iconLabel);
    row->addLayout(info, 1);
    row->addLayout(right);
    row->addWidget(m_remove, 0, Qt::AlignTop);

    connect(m_minutes, &QSpinBox::valueChanged, this, &WatchCard::limitChanged);
    connect(m_remove, &QToolButton::clicked, this, &WatchCard::removeRequested);

    WatchStatus initial;
    initial.exeName = item.exeName;
    initial.limitSecs = item.limitSecs;
    initial.remainingSecs = item.limitSecs;
    setStatus(initial);
}

void WatchCard::setStatus(const WatchStatus& status)
{
    const int limit = std::max(1, status.limitSecs);
    QString state, pill;
    switch (status.state) {
    case WatchState::Idle:
        state = status.remainingSecs == 0 ? "locked" : "idle";
        pill = status.remainingSecs == 0 ? "Used up" : "Not running";
        break;
    case WatchState::Background:
        state = "idle";
        pill = "Paused · no window";
        break;
    case WatchState::Active: {
        const bool low = status.remainingSecs <= std::max(60, limit / 10);
        state = low ? "warn" : "active";
        pill = low ? "Almost up" : "Active";
        break;
    }
    case WatchState::Locked:
        state = "locked";
        pill = "Locked";
        break;
    }

    m_pill->setText(pill);
    setFrozen(status.refillInSecs > 0, status.refillInSecs);
    if (status.refillInSecs > 0) {
        // The monitor's frozen limit is authoritative while the window runs.
        const QSignalBlocker block(m_minutes);
        m_minutes->setValue(std::max(1, status.limitSecs / 60));
    }
    m_time->setText(formatClock(status.remainingSecs));
    m_bar->setRange(0, limit);
    m_bar->setValue(std::clamp(status.remainingSecs, 0, limit));

    if (state != m_state) {
        m_state = state;
        for (QWidget* w : {static_cast<QWidget*>(m_pill), static_cast<QWidget*>(m_time),
                           static_cast<QWidget*>(m_bar)})
            restyle(w, state);
    }
}

void WatchCard::setFrozen(bool frozen, qint64 refillInSecs)
{
    m_refill->setVisible(frozen);
    if (frozen)
        m_refill->setText("Refills in " + formatCountdown(refillInSecs) + " · limit locked until then");

    const QString tip = frozen ? QString("Locked until the budget refills") : QString();
    for (QWidget* w : {static_cast<QWidget*>(m_minus), static_cast<QWidget*>(m_minutes),
                       static_cast<QWidget*>(m_plus)}) {
        w->setEnabled(!frozen);
        w->setToolTip(frozen ? tip : w == m_minutes ? "Session budget"
                                     : w == m_minus ? "Less time" : "More time");
    }
    m_remove->setEnabled(!frozen);
    m_remove->setToolTip(frozen ? tip : QString("Remove from watchlist"));
}
