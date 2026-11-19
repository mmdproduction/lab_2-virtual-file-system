#pragma once

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QString>

namespace ui {

enum class Icon { Folder, File, ArrowUp, Plus, User, Group, Text, Image, Audio, Video, Code, Rename, Trash, Palette, Search };

// Render several sizes so Qt can choose a sharp bitmap on high-DPI screens.
inline QIcon LineIcon(Icon kind, const QColor& color = QColor("#aeb6c4")) {
    QIcon icon;
    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 80};
    const QIcon::Mode modes[] = {QIcon::Normal, QIcon::Disabled, QIcon::Selected};
    for (const int pixels : sizes) {
        for (const QIcon::Mode mode : modes) {
            QPixmap pixmap(pixels, pixels);
            pixmap.fill(Qt::transparent);
            {
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::Antialiasing);
                painter.scale(pixels / 24.0, pixels / 24.0);
                const QColor stroke = mode == QIcon::Disabled ? QColor("#585f6b")
                    : mode == QIcon::Selected ? QColor("#f0f3f8") : color;
                painter.setPen(QPen(stroke, 1.65, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                painter.setBrush(Qt::NoBrush);
                QPainterPath path;
                switch (kind) {
                    case Icon::Folder:
                        path.moveTo(3.5, 8);
                        path.lineTo(3.5, 6.5);
                        path.quadTo(3.5, 5, 5, 5);
                        path.lineTo(9.5, 5);
                        path.lineTo(12, 7.5);
                        path.lineTo(19, 7.5);
                        path.quadTo(20.5, 7.5, 20.5, 9);
                        path.lineTo(20.5, 18);
                        path.quadTo(20.5, 19.5, 19, 19.5);
                        path.lineTo(5, 19.5);
                        path.quadTo(3.5, 19.5, 3.5, 18);
                        path.closeSubpath();
                        path.moveTo(3.5, 10);
                        path.lineTo(20.5, 10);
                        break;
                    case Icon::File:
                        path.moveTo(13.5, 3.5);
                        path.lineTo(6, 3.5);
                        path.lineTo(6, 20.5);
                        path.lineTo(19, 20.5);
                        path.lineTo(19, 9);
                        path.closeSubpath();
                        path.moveTo(13.5, 3.5);
                        path.lineTo(13.5, 9);
                        path.lineTo(19, 9);
                        path.moveTo(9, 13);
                        path.lineTo(16, 13);
                        path.moveTo(9, 16.5);
                        path.lineTo(14, 16.5);
                        break;
                    case Icon::Text:
                        path.addRoundedRect(QRectF(5, 3.5, 14, 17), 1.5, 1.5);
                        path.moveTo(8, 8); path.lineTo(16, 8);
                        path.moveTo(8, 12); path.lineTo(16, 12);
                        path.moveTo(8, 16); path.lineTo(13, 16);
                        break;
                    case Icon::Image:
                        path.addRoundedRect(QRectF(3.5, 4.5, 17, 15), 2, 2);
                        path.addEllipse(QPointF(15.5, 8.5), 1.5, 1.5);
                        path.moveTo(4, 16); path.lineTo(9, 11); path.lineTo(14, 16);
                        path.moveTo(12, 14); path.lineTo(16, 11); path.lineTo(20, 15);
                        break;
                    case Icon::Audio:
                        path.addEllipse(QPointF(7, 17), 3, 2.5);
                        path.addEllipse(QPointF(17, 15), 3, 2.5);
                        path.moveTo(10, 17); path.lineTo(10, 5); path.lineTo(20, 3);
                        path.lineTo(20, 15);
                        path.moveTo(10, 9); path.lineTo(20, 7);
                        break;
                    case Icon::Video:
                        path.addRoundedRect(QRectF(3, 5, 18, 14), 2, 2);
                        path.moveTo(10, 9); path.lineTo(16, 12); path.lineTo(10, 15); path.closeSubpath();
                        break;
                    case Icon::Code:
                        path.moveTo(8, 7); path.lineTo(3, 12); path.lineTo(8, 17);
                        path.moveTo(16, 7); path.lineTo(21, 12); path.lineTo(16, 17);
                        path.moveTo(14, 4); path.lineTo(10, 20);
                        break;
                    case Icon::Rename:
                        path.moveTo(4, 20); path.lineTo(5, 14); path.lineTo(16, 3);
                        path.lineTo(21, 8); path.lineTo(10, 19); path.closeSubpath();
                        path.moveTo(13.5, 5.5); path.lineTo(18.5, 10.5);
                        break;
                    case Icon::Trash:
                        path.moveTo(4, 6); path.lineTo(20, 6);
                        path.moveTo(9, 6); path.lineTo(9, 3); path.lineTo(15, 3); path.lineTo(15, 6);
                        path.moveTo(6, 6); path.lineTo(7, 21); path.lineTo(17, 21); path.lineTo(18, 6);
                        path.moveTo(10, 10); path.lineTo(10, 17);
                        path.moveTo(14, 10); path.lineTo(14, 17);
                        break;
                    case Icon::Search:
                        path.addEllipse(QPointF(10, 10), 6, 6);
                        path.moveTo(14.5, 14.5); path.lineTo(21, 21);
                        break;
                    case Icon::Palette:
                        path.addRoundedRect(QRectF(3, 3, 7, 7), 1, 1);
                        path.addRoundedRect(QRectF(14, 3, 7, 7), 1, 1);
                        path.addRoundedRect(QRectF(3, 14, 7, 7), 1, 1);
                        path.addEllipse(QRectF(14, 14, 7, 7));
                        break;
                    case Icon::ArrowUp:
                        path.moveTo(6, 11);
                        path.lineTo(12, 5);
                        path.lineTo(18, 11);
                        path.moveTo(12, 5);
                        path.lineTo(12, 20);
                        break;
                    case Icon::Plus:
                        path.moveTo(12, 5);
                        path.lineTo(12, 19);
                        path.moveTo(5, 12);
                        path.lineTo(19, 12);
                        break;
                    case Icon::Group:
                        path.addEllipse(QPointF(8.5, 8), 3.2, 3.2);
                        path.moveTo(2.5, 20);
                        path.lineTo(2.5, 18.5);
                        path.cubicTo(2.5, 12.5, 14.5, 12.5, 14.5, 18.5);
                        path.lineTo(14.5, 20);
                        path.moveTo(16, 5);
                        path.cubicTo(21, 5, 21, 11, 16, 11);
                        path.moveTo(17, 14);
                        path.cubicTo(21, 14, 21.5, 17, 21.5, 20);
                        break;
                    case Icon::User:
                        path.addEllipse(QPointF(12, 7.5), 3.5, 3.5);
                        path.moveTo(5, 20);
                        path.lineTo(5, 18.5);
                        path.cubicTo(5, 12, 19, 12, 19, 18.5);
                        path.lineTo(19, 20);
                        break;
                }
                painter.drawPath(path);
            }
            icon.addPixmap(pixmap, mode);
        }
    }
    return icon;
}

inline QPalette DarkPalette() {
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#111315"));
    palette.setColor(QPalette::WindowText, QColor("#e9edf3"));
    palette.setColor(QPalette::Base, QColor("#181b1f"));
    palette.setColor(QPalette::AlternateBase, QColor("#1c2025"));
    palette.setColor(QPalette::Text, QColor("#e9edf3"));
    palette.setColor(QPalette::Button, QColor("#23272e"));
    palette.setColor(QPalette::ButtonText, QColor("#e9edf3"));
    palette.setColor(QPalette::Highlight, QColor("#303d52"));
    palette.setColor(QPalette::HighlightedText, QColor("#f4f6fa"));
    palette.setColor(QPalette::ToolTipBase, QColor("#242a33"));
    palette.setColor(QPalette::ToolTipText, QColor("#e9edf3"));
    palette.setColor(QPalette::PlaceholderText, QColor("#7b8491"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#646c79"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#646c79"));
    return palette;
}

inline QString DarkStyleSheet() {
    return QString::fromUtf8(R"(
        QWidget { color: #e9edf3; }
        QMainWindow, QDialog { background-color: #111315; }
        QLabel#title { font-size: 22px; font-weight: 600; }
        QLabel#subtitle { color: #89929f; }
        QWidget#user_badge { background: #1b1f24; border: 1px solid #2c323b; border-radius: 7px; }
        QLabel#user_label { color: #bac2cf; }
        QLabel#section_title { color: #7f8997; font-size: 11px; }
        QLabel#current_path { color: #cdd5e1; font-weight: 500; padding: 3px 0; }
        QPushButton { background: #20252b; color: #ccd3df; padding: 8px 15px;
            border: 1px solid #323943; border-radius: 6px; }
        QPushButton:hover { background: #2a3039; border-color: #46505e; }
        QPushButton:pressed { background: #181c21; }
        QPushButton:focus { border-color: #7188ac; }
        QPushButton:disabled { background: #191d22; color: #606a78; border-color: #2b3038; }
        QPushButton#create_directory_button, QPushButton#apply_policy_button { background: #e1e6ef; color: #161b23; border-color: #e1e6ef; }
        QPushButton#create_directory_button:hover, QPushButton#apply_policy_button:hover { background: #f3f5fa; border-color: #f3f5fa; }
        QPushButton#create_directory_button:pressed, QPushButton#apply_policy_button:pressed { background: #c2cad8; }
        QPushButton#create_directory_button:disabled, QPushButton#apply_policy_button:disabled {
            background: #191d22; color: #606a78; border-color: #2b3038;
        }
        QTreeView, QTableView, QListView { background: #181b1f; alternate-background-color: #1c2025;
            color: #d7dde7; selection-background-color: #303d52; selection-color: #f4f6fa;
            border: 1px solid #2b313a; border-radius: 7px; padding: 8px; outline: none; }
        QTreeView::item { height: 35px; }
        QListView::item { height: 46px; padding-left: 4px; }
        QLabel#dialog_title { font-size: 24px; font-weight: 600; }
        QLabel#muted { color: #89929f; }
        QTabWidget::pane { border: 1px solid #2b313a; border-radius: 7px; }
        QTabBar::tab { background: #181b1f; color: #89929f; padding: 12px 18px;
            border-bottom: 2px solid transparent; }
        QTabBar::tab:selected { color: #e9edf3; border-bottom-color: #aeb6c4; }
        QTabBar::tab:hover { background: #232a34; }
        QCheckBox { spacing: 9px; }
        QCheckBox::indicator:unchecked, QAbstractItemView::indicator:unchecked {
            width: 13px; height: 13px; border: 1px solid #58616e;
            border-radius: 2px; background: #181b1f;
        }
        QTreeView::item:hover, QTableView::item:hover, QListView::item:hover { background: #232a34; }
        QTreeView::item:selected, QTableView::item:selected, QListView::item:selected { background: #303d52; }
        QHeaderView::section { background: #1d2229; color: #8994a5; padding: 11px 9px;
            border: none; border-bottom: 1px solid #2b313a; }
        QLabel#empty_state { color: #89929f; padding: 25px; }
        QToolButton { background: #20252b; border: 1px solid #323943; border-radius: 5px; padding: 7px; }
        QToolButton:hover { background: #303743; border-color: #536074; }
        QToolButton:disabled { background: #181c21; border-color: #272d35; }
        QMenuBar { background: #111315; color: #cdd5e1; padding: 3px 12px; }
        QMenuBar::item { padding: 5px 9px; }
        QMenuBar::item:selected { background: #303d52; border-radius: 4px; }
        QMenu { background: #1c2128; color: #e9edf3; border: 1px solid #3b4553; padding: 5px; }
        QMenu::item { padding: 9px 24px 9px 12px; border-radius: 4px; }
        QMenu::item:selected { background: #303d52; }
        QPushButton:checked { background: #303d52; border-color: #899bb6; }
        QComboBox { background: #20252b; color: #e9edf3; padding: 7px 24px 7px 9px;
            border: 1px solid #3b4553; border-radius: 5px; }
        QComboBox QAbstractItemView { background: #20252b; color: #e9edf3;
            selection-background-color: #303d52; border: 1px solid #3b4553; }
        QLineEdit { background: #181b1f; color: #e9edf3; padding: 8px;
            border: 1px solid #3b4553; border-radius: 5px; }
        QLineEdit:focus { border-color: #7188ac; }
        QToolTip { background: #242a33; color: #e9edf3; border: 1px solid #3b4553; padding: 5px; }
        QStatusBar { color: #7f8997; border-top: 1px solid #272e37; }
        QSplitter::handle { background: #111315; }
        QScrollBar:vertical { background: #181b1f; width: 9px; margin: 2px; }
        QScrollBar::handle:vertical { background: #3a424f; border-radius: 3px; min-height: 25px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }
    )");
}

}  // namespace ui
