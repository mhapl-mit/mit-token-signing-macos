/*
 * Chrome Token Signing Native Host
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <QApplication>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QVBoxLayout>

// Common look of the dialogs: heading with the logo on the base colour, content over the full
// width, buttons in a footer on the window colour. Colours come from the palette, so dark
// themes keep working.
namespace DialogStyle {

struct Layout {
    QVBoxLayout *content;
    QHBoxLayout *footer;
};

// The application icon is the logo with a wide empty border, crop it to the drawing
inline QPixmap logo(int height)
{
    QImage image = QApplication::windowIcon().pixmap(256, 256).toImage().convertToFormat(QImage::Format_ARGB32);
    QRect drawn;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            QRgb pixel = image.pixel(x, y);
            if (qAlpha(pixel) > 32 && qMin(qMin(qRed(pixel), qGreen(pixel)), qBlue(pixel)) < 224)
                drawn |= QRect(x, y, 1, 1);
        }
    }
    if (drawn.isEmpty())
        drawn = image.rect();
    return QPixmap::fromImage(image.copy(drawn).scaledToHeight(height, Qt::SmoothTransformation));
}

inline Layout setup(QDialog *dialog, const QString &heading)
{
    // The heading says what the dialog is for, the title is the application name only
    dialog->setWindowTitle(QApplication::applicationDisplayName());

    QVBoxLayout *root = new QVBoxLayout(dialog);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    QWidget *body = new QWidget(dialog);
    body->setAutoFillBackground(true);
    body->setBackgroundRole(QPalette::Base);
    body->setStyleSheet(QStringLiteral(
        "QLineEdit { padding: 5px 8px; border: 1px solid palette(mid); border-radius: 4px; background: palette(base); }"
        "QLineEdit:focus { border: 2px solid palette(highlight); padding: 4px 7px; }"));
    QVBoxLayout *content = new QVBoxLayout(body);
    content->setContentsMargins(24, 20, 24, 24);
    content->setSpacing(6);

    QHBoxLayout *header = new QHBoxLayout;
    header->setSpacing(16);
    QLabel *title = new QLabel(heading, body);
    QFont font = title->font();
    font.setPointSizeF(font.pointSizeF() * 1.5);
    font.setWeight(QFont::DemiBold);
    title->setFont(font);
    header->addWidget(title, 1, Qt::AlignVCenter);
    QLabel *brand = new QLabel(body);
    brand->setPixmap(logo(36));
    header->addWidget(brand, 0, Qt::AlignTop);
    content->addLayout(header);
    content->addSpacing(6);
    root->addWidget(body, 1);

    QFrame *line = new QFrame(dialog);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setForegroundRole(QPalette::Midlight);
    root->addWidget(line);

    QWidget *footer = new QWidget(dialog);
    footer->setAutoFillBackground(true);
    footer->setBackgroundRole(QPalette::Window);
    // The default button is the main action
    footer->setStyleSheet(QStringLiteral(
        "QPushButton { min-width: 72px; padding: 6px 16px; border: 1px solid palette(mid); border-radius: 4px; background: palette(base); }"
        "QPushButton:hover { background: palette(midlight); }"
        "QPushButton:default { color: palette(highlighted-text); background: palette(highlight); border-color: palette(highlight); }"
        "QPushButton:default:hover { border-color: palette(dark); }"
        "QPushButton:default:disabled { color: palette(mid); background: palette(midlight); border-color: palette(midlight); }"));
    QHBoxLayout *footerLayout = new QHBoxLayout(footer);
    footerLayout->setContentsMargins(24, 14, 24, 14);
    root->addWidget(footer);
    return {content, footerLayout};
}

// Labels on the base colour are drawn with the Text role, set both roles
inline void setTextColor(QLabel *label, const QColor &color)
{
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, color);
    palette.setColor(QPalette::Text, color);
    label->setPalette(palette);
}

// Less important text: certificate name, document count
inline void secondary(QLabel *label)
{
    const QPalette &palette = label->palette();
    QColor color = palette.color(QPalette::Text);
    color.setAlphaF(0.65);
    setTextColor(label, color);
}

inline void error(QLabel *label)
{
    setTextColor(label, label->palette().color(QPalette::Base).lightness() > 128 ? QColor(196, 43, 28) : QColor(255, 153, 164));
}

}
