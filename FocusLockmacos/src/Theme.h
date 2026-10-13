#pragma once

#include <QColor>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QString>

// Dark palette in the spirit of Rize: near-black canvas, soft raised cards,
// a violet accent, and status colours used sparingly.
namespace theme {

inline const QColor kBackground{"#121212"};
inline const QColor kSurface{"#1B1B1F"};
inline const QColor kSurfaceRaised{"#232329"};
inline const QColor kText{"#EDEDF0"};
inline const QColor kMuted{"#8B8B96"};
inline const QColor kAccent{"#7C6CFF"};

inline QPalette palette()
{
    QPalette p;
    p.setColor(QPalette::Window, kBackground);
    p.setColor(QPalette::WindowText, kText);
    p.setColor(QPalette::Base, kSurface);
    p.setColor(QPalette::AlternateBase, kSurfaceRaised);
    p.setColor(QPalette::Text, kText);
    p.setColor(QPalette::Button, kSurfaceRaised);
    p.setColor(QPalette::ButtonText, kText);
    p.setColor(QPalette::Highlight, kAccent);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::ToolTipBase, kSurfaceRaised);
    p.setColor(QPalette::ToolTipText, kText);
    p.setColor(QPalette::PlaceholderText, QColor("#6E6E79"));
    p.setColor(QPalette::Link, QColor("#A08CFF"));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, QColor("#5A5A63"));
    return p;
}

inline QString styleSheet()
{
    return R"(
QMainWindow, #root { background: #121212; }
QWidget { color: #EDEDF0; font-family: ".AppleSystemUIFont", "Helvetica Neue"; font-size: 13.5pt; }
QToolTip { background: #232329; color: #EDEDF0; border: 1px solid #33333B; border-radius: 6px; padding: 4px 8px; }

#title { font-family: ".AppleSystemUIFont", "Helvetica Neue"; font-size: 26.5pt; font-weight: 700; }
#subtitle { color: #8B8B96; }
#section { color: #6E6E79; font-size: 10.5pt; font-weight: 700; }
#footer { color: #6E6E79; font-size: 11.5pt; }

QScrollArea, #listHost { background: transparent; border: none; }
QScrollArea > QWidget > QWidget { background: transparent; }
QScrollBar:vertical { background: transparent; width: 8px; margin: 0; }
QScrollBar::handle:vertical { background: #2E2E36; border-radius: 4px; min-height: 32px; }
QScrollBar::handle:vertical:hover { background: #3A3A44; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

#stat, #card { background: #1B1B1F; border: 1px solid #26262C; border-radius: 14px; }
#card:hover { border-color: #34343D; }
#statValue { font-family: ".AppleSystemUIFont", "Helvetica Neue"; font-size: 24pt; font-weight: 600; }
#statCaption { color: #8B8B96; font-size: 12pt; }

#emptyState { background: transparent; border: 1px dashed #2E2E36; border-radius: 14px; }
#emptyTitle { font-size: 16pt; font-weight: 600; }
#emptyBody { color: #8B8B96; }

#appIcon { background: #232329; border-radius: 12px; }
#cardTitle { font-size: 14.5pt; font-weight: 600; }
#cardPath { color: #6E6E79; font-size: 11.5pt; }
#refill { color: #A08CFF; font-size: 11.5pt; }

#pill { color: #8B8B96; background: #232329; border-radius: 9px; padding: 2px 9px; font-size: 10.5pt; font-weight: 600; }
#pill[state="active"] { color: #3DD68C; background: rgba(61, 214, 140, 0.12); }
#pill[state="warn"]   { color: #F5A524; background: rgba(245, 165, 36, 0.12); }
#pill[state="locked"] { color: #F2555A; background: rgba(242, 85, 90, 0.14); }

#time { font-family: "SF Mono", "Menlo"; font-size: 20pt; font-weight: 600; color: #8B8B96; }
#time[state="active"] { color: #EDEDF0; }
#time[state="warn"]   { color: #F5A524; }
#time[state="locked"] { color: #F2555A; }

QProgressBar { background: #26262C; border: none; border-radius: 2px; }
QProgressBar::chunk { background: #4A4A55; border-radius: 2px; }
QProgressBar[state="active"]::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #7C6CFF, stop:1 #A08CFF); }
QProgressBar[state="warn"]::chunk   { background: #F5A524; }
QProgressBar[state="locked"]::chunk { background: #F2555A; }

QSpinBox { background: #232329; border: 1px solid #2E2E36; border-radius: 8px; padding: 0 4px; min-height: 26px; selection-background-color: #7C6CFF; }
QSpinBox:focus { border-color: #7C6CFF; }
/* Hidden steppers (the card has its own -/+ buttons). Using NoButtons instead collapses the editor to 1px under a stylesheet. */
QSpinBox::up-button, QSpinBox::down-button { width: 0; border: none; }
#stepper { background: #232329; border: 1px solid #2E2E36; border-radius: 8px; color: #B4B4BE; min-width: 26px; min-height: 26px; font-weight: 600; }
#stepper:hover { background: #2C2C34; color: #FFFFFF; }
#remove { background: transparent; border: none; color: #6E6E79; border-radius: 8px; padding: 4px 7px; }
#remove:hover { background: rgba(242, 85, 90, 0.12); color: #F2555A; }
QSpinBox:disabled, #stepper:disabled { background: #18181C; border-color: #222228; color: #55555F; }
#remove:disabled { color: #3A3A44; }

QPushButton#primary { background: #7C6CFF; color: #FFFFFF; border: none; border-radius: 10px; padding: 9px 18px; font-weight: 600; }
QPushButton#primary:hover { background: #8F81FF; }
QPushButton#primary:pressed { background: #6A5AF0; }
QPushButton#primary:disabled { background: #3A3550; color: #9A97AA; }

QMenu { background: #1B1B1F; border: 1px solid #2E2E36; border-radius: 8px; padding: 4px; }
QMenu::item { padding: 6px 18px; border-radius: 6px; }
QMenu::item:selected { background: #2A2A33; }
QMenu::separator { height: 1px; background: #2E2E36; margin: 4px 8px; }

#overlayCard { background: #17171B; border: 1px solid #2F2F38; border-radius: 20px; }
#overlayTitle { font-family: ".AppleSystemUIFont", "Helvetica Neue"; font-size: 29.5pt; font-weight: 700; }
#overlayBody { color: #B4B4BE; font-size: 14pt; }
#overlayHint { color: #6E6E79; font-size: 11.5pt; }
#overlayBadge { color: #F2555A; background: rgba(242, 85, 90, 0.14); border-radius: 10px; padding: 3px 10px; font-size: 10.5pt; font-weight: 700; }
)";
}

// Violet rounded square with a white padlock, drawn at every common icon size.
inline QIcon appIcon()
{
    QIcon icon;
    for (int size : {16, 24, 32, 48, 64, 128, 256}) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = size;

        QLinearGradient bg(0, 0, s, s);
        bg.setColorAt(0, QColor("#8F7CFF"));
        bg.setColorAt(1, QColor("#5B4BE0"));
        p.setPen(Qt::NoPen);
        p.setBrush(bg);
        p.drawRoundedRect(QRectF(0, 0, s, s), s * 0.24, s * 0.24);

        QPen shackle(Qt::white, s * 0.085);
        shackle.setCapStyle(Qt::FlatCap);
        p.setPen(shackle);
        p.setBrush(Qt::NoBrush);
        const QRectF arc(s * 0.34, s * 0.20, s * 0.32, s * 0.34);
        p.drawArc(arc, 0, 180 * 16);
        p.drawLine(QPointF(arc.left(), arc.center().y()), QPointF(arc.left(), s * 0.46));
        p.drawLine(QPointF(arc.right(), arc.center().y()), QPointF(arc.right(), s * 0.46));

        p.setPen(Qt::NoPen);
        p.setBrush(Qt::white);
        p.drawRoundedRect(QRectF(s * 0.26, s * 0.44, s * 0.48, s * 0.36), s * 0.08, s * 0.08);
        p.setBrush(QColor("#6A5AF0"));
        p.drawEllipse(QPointF(s * 0.5, s * 0.61), s * 0.05, s * 0.05);
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

} // namespace theme
