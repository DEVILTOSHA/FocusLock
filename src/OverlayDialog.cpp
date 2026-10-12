#include "OverlayDialog.h"

#include "WatchTypes.h"
#include "WinUtil.h"

#include <QCloseEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kFollowMs = 200;
constexpr int kCloseGraceMs = 15000;
} // namespace

OverlayDialog::OverlayDialog(const QString& appName, const QIcon& icon, int limitSecs,
                             qint64 refillInSecs, QWidget* parent)
    : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , m_appName(appName)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(QString("FocusLock — %1").arg(appName));

    m_card = new QFrame(this);
    m_card->setObjectName("overlayCard");
    m_card->setFixedWidth(440);

    auto* shadow = new QGraphicsDropShadowEffect(m_card);
    shadow->setBlurRadius(48);
    shadow->setOffset(0, 14);
    shadow->setColor(QColor(0, 0, 0, 170));
    m_card->setGraphicsEffect(shadow);

    auto* iconLabel = new QLabel;
    iconLabel->setPixmap(icon.pixmap(48, 48));

    auto* badge = new QLabel("LOCKED");
    badge->setObjectName("overlayBadge");

    auto* top = new QHBoxLayout;
    top->addWidget(iconLabel);
    top->addStretch();
    top->addWidget(badge, 0, Qt::AlignTop);

    auto* title = new QLabel("Time's up");
    title->setObjectName("overlayTitle");

    auto* body = new QLabel(
        QString("You've used your %1 budget for <b>%2</b>. It refills in %3 — until then "
                "this screen stays up whenever the app is open.")
            .arg(formatBudget(limitSecs), appName.toHtmlEscaped(),
                 formatCountdown(refillInSecs)));
    body->setObjectName("overlayBody");
    body->setTextFormat(Qt::RichText);
    body->setWordWrap(true);

    m_closeButton = new QPushButton(QString("Close %1").arg(appName));
    m_closeButton->setObjectName("primary");
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setAutoDefault(false);
    connect(m_closeButton, &QPushButton::clicked, this, &OverlayDialog::requestTargetClose);

    m_hint = new QLabel(QString("%1 can still ask you to save before it quits.").arg(appName));
    m_hint->setObjectName("overlayHint");
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setWordWrap(true);

    auto* card = new QVBoxLayout(m_card);
    card->setContentsMargins(32, 28, 32, 24);
    card->setSpacing(0);
    card->addLayout(top);
    card->addSpacing(18);
    card->addWidget(title);
    card->addSpacing(6);
    card->addWidget(body);
    card->addSpacing(22);
    card->addWidget(m_closeButton);
    card->addSpacing(12);
    card->addWidget(m_hint);

    // Card centred in the window; the margin leaves room for the drop shadow.
    auto* row = new QHBoxLayout;
    row->addStretch();
    row->addWidget(m_card);
    row->addStretch();
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(28, 28, 28, 28);
    outer->addStretch();
    outer->addLayout(row);
    outer->addStretch();

    m_follow.setInterval(kFollowMs);
    connect(&m_follow, &QTimer::timeout, this, &OverlayDialog::reposition);
}

void OverlayDialog::setPids(const QList<quint32>& pids)
{
    m_pids = pids;
}

void OverlayDialog::release()
{
    m_released = true;
    m_follow.stop();
    close();
}

void OverlayDialog::reject()
{
    if (m_released)
        QDialog::reject();
}

void OverlayDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    reposition();
    m_follow.start();
}

void OverlayDialog::closeEvent(QCloseEvent* event)
{
    if (m_released)
        event->accept();
    else
        event->ignore();
}

void OverlayDialog::paintEvent(QPaintEvent*)
{
    if (m_compact)
        return;
    QPainter painter(this);
    painter.fillRect(rect(), QColor(10, 10, 12, 225));
}

void OverlayDialog::setCompact(bool compact)
{
    if (m_compact == compact)
        return;
    m_compact = compact;
    update();
}

void OverlayDialog::reposition()
{
    const quintptr self = winId();
    const auto target = winutil::findMainWindow(m_pids);
    const bool targetVisible = target && !target->minimized;

    if (targetVisible && !m_closing) {
        setCompact(false);
        winutil::placeTopmost(self, target->rect, true);
        // Alt-tabbing into the covered app shouldn't let keystrokes reach it.
        if (winutil::foregroundWindow() == target->handle) {
            raise();
            activateWindow();
        }
        return;
    }

    setCompact(true);
    const QSize compactSize = layout()->sizeHint();
    if (size() != compactSize)
        resize(compactSize);

    const QRect own = winutil::windowRect(self);
    QPoint topLeft;
    if (targetVisible) {
        // Closing: sit along the top edge so the app's own save prompt is clickable.
        topLeft = QPoint(target->rect.center().x() - own.width() / 2, target->rect.top());
    } else {
        const QRect area = winutil::workArea(target ? target->handle : 0);
        topLeft = area.center() - QPoint(own.width() / 2, own.height() / 2);
    }
    winutil::placeTopmost(self, QRect(topLeft, own.size()), false);
}

void OverlayDialog::requestTargetClose()
{
    m_closing = true;
    m_closeButton->setEnabled(false);
    m_closeButton->setText(QString("Closing %1…").arg(m_appName));
    m_hint->setText(QString("Waiting for %1 to exit.").arg(m_appName));
    winutil::requestClose(m_pids);
    reposition();

    // If the app is still around (e.g. the user cancelled a save prompt), lock it again.
    QTimer::singleShot(kCloseGraceMs, this, [this] {
        m_closing = false;
        m_closeButton->setEnabled(true);
        m_closeButton->setText(QString("Close %1").arg(m_appName));
        m_hint->setText(QString("%1 is still open. Finish any save prompts, then close it.")
                            .arg(m_appName));
        reposition();
    });
}
