/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2020  Prince Gupta <jagannatharjun11@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * In addition, as a special exception, the copyright holders give permission to
 * link this program with the OpenSSL project's "OpenSSL" library (or with
 * modified versions of it that use the same license as the "OpenSSL" library),
 * and distribute the linked executables. You must obey the GNU General Public
 * License in all respects for all of the code used other than "OpenSSL".  If you
 * modify file(s), you may extend this exception to your version of the file(s),
 * but you are not obligated to do so. If you do not wish to do so, delete this
 * exception statement from your version.
 */

#include "progressbarpainter.h"

#include <QPainter>
#include <QPalette>
#include <QStyleOptionProgressBar>
#include <QStyleOptionViewItem>

#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
#include <QProxyStyle>
#endif

#include "base/global.h"
#include "gui/uithememanager.h"

ProgressBarPainter::ProgressBarPainter(QObject *parent)
    : QObject(parent)
{
#if (defined(Q_OS_WIN) || defined(Q_OS_MACOS))
    auto *fusionStyle = new QProxyStyle(u"fusion"_s);
    fusionStyle->setParent(&m_dummyProgressBar);
    m_dummyProgressBar.setStyle(fusionStyle);
#endif

    applyUITheme();
    connect(UIThemeManager::instance(), &UIThemeManager::themeChanged, this, &ProgressBarPainter::applyUITheme);
}

void ProgressBarPainter::paint(QPainter *painter, const QStyleOptionViewItem &option, const QString &text, const int progress, const QColor &color) const
{
    // Prepare the text bounding rect on the right
    const QFontMetrics fontMetrics = option.fontMetrics;
    const int textMargin = 6;      // Space between progress bar and text
    const int rightPadding = 6;    // Space between text and the right cell boundary
    const int textWidth = fontMetrics.horizontalAdvance(text);

    // Calculate layout geometries
    QRect cellRect = option.rect;
    
    // Position textRect, offsetting it from the right boundary by rightPadding
    QRect textRect = cellRect;
    textRect.setRight(cellRect.right() - rightPadding);
    textRect.setLeft(textRect.right() - textWidth);

    // Position barRect to stop left of the text, incorporating textMargin
    QRect barRect = cellRect;
    barRect.setRight(textRect.left() - textMargin);

    // Clamp the progress bar height to 8px and center it vertically
    const int maxHeight = 8;
    if (barRect.height() > maxHeight)
    {
        const int topMargin = (barRect.height() - maxHeight) / 2;
        barRect.setTop(barRect.top() + topMargin);
        barRect.setHeight(maxHeight);
    }

    // Configure progress bar style option
    QStyleOptionProgressBar styleOption;
    styleOption.initFrom(&m_dummyProgressBar);
    styleOption.maximum = 100;
    styleOption.minimum = 0;
    styleOption.progress = progress;
    styleOption.textVisible = false; // Hide default embedded text
    styleOption.rect = barRect;
    styleOption.state = option.state | QStyle::State_Horizontal;

    const bool isEnabled = option.state.testFlag(QStyle::State_Enabled);
    styleOption.palette.setCurrentColorGroup(isEnabled ? QPalette::Active : QPalette::Disabled);

    if (color.isValid())
    {
        styleOption.palette.setColor(QPalette::Highlight, color);
    }
    else if (m_chunkColor.isValid())
    {
        styleOption.palette.setColor(QPalette::Highlight, m_chunkColor);
    }

    // Render components
    painter->save();
    const QStyle *style = m_dummyProgressBar.style();

    // Draw row background
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw 8px progress bar without text
    style->drawControl(QStyle::CE_ProgressBar, &styleOption, painter, &m_dummyProgressBar);

    // Draw text manually on the right side
    const QPalette::ColorRole textRole = (option.state & QStyle::State_Selected)
        ? QPalette::HighlightedText
        : QPalette::Text;
    painter->setPen(option.palette.color(styleOption.palette.currentColorGroup(), textRole));
    painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, text);

    painter->restore();
}

void ProgressBarPainter::applyUITheme()
{
    m_chunkColor = UIThemeManager::instance()->getColor(u"ProgressBar"_s);
}
