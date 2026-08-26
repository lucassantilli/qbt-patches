/*
 * Bittorrent Client using Qt and libtorrent.
 * Copyright (C) 2025  Vladimir Golovnev <glassez@yandex.ru>
 * Copyright (C) 2006  Christophe Dumez <chris@qbittorrent.org>
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

#include "transferlistdelegate.h"

#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QModelIndex>

#include "base/preferences.h"
#include "transferlistmodel.h"

TransferListDelegate::TransferListDelegate(QObject *parent)
    : QStyledItemDelegate(parent)
{
}

QSize TransferListDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    // Reimplementing sizeHint() because the 'name' column contains text+icon.
    // When that WHOLE column goes out of view(eg user scrolls horizontally)
    // the rows shrink if the text's height is smaller than the icon's height.
    // This happens because icon from the 'name' column is no longer drawn.

    if (m_nameColHeight == -1)
    {
        const QModelIndex nameColumn = index.sibling(index.row(), TransferListModel::TR_NAME);
        m_nameColHeight = QStyledItemDelegate::sizeHint(option, nameColumn).height();
    }

    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(std::max(m_nameColHeight, size.height()));
    return size;
}

void TransferListDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    switch (index.column())
    {
    case TransferListModel::TR_PROGRESS:
        {
            using namespace BitTorrent;

            const auto isEnableState = [](const TorrentState state) -> bool
            {
                switch (state)
                {
                case TorrentState::Error:
                case TorrentState::StoppedDownloading:
                case TorrentState::Unknown:
                    return false;
                default:
                    return true;
                }
            };

            const int progress = static_cast<int>(index.data(TransferListModel::UnderlyingDataRole).toReal());

            const QModelIndex statusIndex = index.siblingAtColumn(TransferListModel::TR_STATUS);
            const auto torrentState = statusIndex.data(TransferListModel::UnderlyingDataRole).value<TorrentState>();

            QStyleOptionViewItem customOption {option};
            customOption.state.setFlag(QStyle::State_Enabled, isEnableState(torrentState));

            const QColor color = Preferences::instance()->getProgressBarFollowsTextColor() ? index.data(Qt::ForegroundRole).value<QColor>() : QColor();

            m_progressBarPainter.paint(painter, customOption, index.data().toString(), progress, color);
        }
        break;

    case TransferListModel::TR_STATUS:
        {
            using BitTorrent::TorrentState;

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            // Fetch state, text, and row selection color
            const auto torrentState = index.data(TransferListModel::UnderlyingDataRole).value<TorrentState>();
            const QString statusText = index.data(Qt::DisplayRole).toString();
            const QColor foregroundColor = index.data(Qt::ForegroundRole).value<QColor>();

            // 1. Calculate fixed badge geometry (used by ALL states to ensure uniform alignment)
            const int horizontalPadding = 12; // 6px padding on left & right
            const int textWidth = option.fontMetrics.horizontalAdvance(statusText);
            const int badgeWidth = textWidth + horizontalPadding;
            const int badgeHeight = option.rect.height() - 8;

            // Center the badge rect inside the column cell
            const int paddingLeft = 5;
            const int badgeX = option.rect.x() + paddingLeft;
            const int badgeY = option.rect.y() + (option.rect.height() - badgeHeight) / 2;
            const QRect badgeRect(badgeX, badgeY, badgeWidth, badgeHeight);

            // 2. State checking & styling setup
            const bool isStalled = (torrentState == TorrentState::StalledDownloading 
                                 || torrentState == TorrentState::StalledUploading);
                                 || torrentState == TorrentState::StoppedDownloading);
                                 || torrentState == TorrentState::StoppedUploading);

            const QColor themeColor = foregroundColor.isValid() ? foregroundColor : option.palette.text().color();
            const int radius = 4;

            QPainterPath path;
            path.addRoundedRect(badgeRect, radius, radius);

            if (isStalled) {
                // Stalled: No background fill, no border.
                // Just set the pen color so the text matches the theme.
                painter->setPen(themeColor);
            }
            else {
                // Active: Fill the background solid (no border stroke).
                painter->fillPath(path, themeColor);
                
                // Set the pen color so the text is white.
                painter->setPen(Qt::white);
            }

            // Draw text perfectly aligned in the center of the invisible bounding box
            painter->drawText(badgeRect, Qt::AlignCenter, statusText);

            painter->restore();
        }
        break;

    default:
        QStyledItemDelegate::paint(painter, option, index);
        break;
    }
}
