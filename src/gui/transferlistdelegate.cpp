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
    case TransferListModel::TR_STATUS:
        {
            using BitTorrent::TorrentState;

            // Draw base cell selection highlight and default background
            QStyledItemDelegate::paint(painter, option, index);

            const auto torrentState = index.data(TransferListModel::UnderlyingDataRole).value<TorrentState>();
            const QString statusText = index.data(Qt::DisplayRole).toString();
            const QColor statusColor = index.data(Qt::ForegroundRole).value<QColor>();

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            // Determine colors according to state requirements
            QColor badgeColor;
            const QColor labelColor = option.palette.color(QPalette::Active, QPalette::Dark);

            switch (torrentState)
            {
            // 1. Cell background color (respecting AlternateBase)
            case TorrentState::StalledDownloading:
            case TorrentState::StalledUploading:
                {
                    const bool isAlternate = (option.features & QStyleOptionViewItem::Alternate);
                    const QPalette::ColorRole bgRole = isAlternate ? QPalette::AlternateBase : QPalette::Base;
                    badgeColor = option.palette.color(QPalette::Active, bgRole);
                }
                break;

            // 2. QPalette::WindowText
            case TorrentState::Downloading:
            case TorrentState::ForcedDownloading:
            case TorrentState::Uploading:
            case TorrentState::ForcedUploading:
                badgeColor = option.palette.color(QPalette::Active, QPalette::WindowText);
                break;

            // 3. Theme statusColor
            case TorrentState::Moving:
            case TorrentState::MissingFiles:
            case TorrentState::Error:
            case TorrentState::StoppedDownloading:
            case TorrentState::StoppedUploading:
            case TorrentState::DownloadingMetadata:
            case TorrentState::ForcedDownloadingMetadata:
            case TorrentState::QueuedDownloading:
            case TorrentState::QueuedUploading:
            case TorrentState::CheckingDownloading:
            case TorrentState::CheckingUploading:
            case TorrentState::CheckingResumeData:
            default:
                badgeColor = statusColor.isValid() ? statusColor : option.palette.color(QPalette::Active, QPalette::Text);
                break;
            }

            // --- Badge Rendering Logic ---
            const int horizontalPadding = 12;
            const int textWidth = option.fontMetrics.horizontalAdvance(statusText);
            const int badgeWidth = textWidth + horizontalPadding;
            const int badgeHeight = option.rect.height() - 8;

            const int paddingLeft = 5;
            const int badgeX = option.rect.x() + paddingLeft;
            const int badgeY = option.rect.y() + (option.rect.height() - badgeHeight) / 2;
            const QRect badgeRect(badgeX, badgeY, badgeWidth, badgeHeight);

            const int radius = 4;
            QPainterPath path;
            path.addRoundedRect(badgeRect, radius, radius);

            // Paint badge background and text label
            painter->fillPath(path, badgeColor);
            painter->setPen(labelColor);
            painter->drawText(badgeRect, Qt::AlignCenter, statusText);

            painter->restore();
        }
        break;

    default:
        QStyledItemDelegate::paint(painter, option, index);
        break;
    }
}
