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

            const auto torrentState = index.data(TransferListModel::UnderlyingDataRole).value<TorrentState>();
            const QString statusText = index.data(Qt::DisplayRole).toString();
            const QColor statusColor = index.data(Qt::ForegroundRole).value<QColor>();

            // 1. Paint background ONLY (strip text completely from the base delegate to prevent duplication)
            QStyleOptionViewItem baseOption = option;
            baseOption.text = QString();
            QStyledItemDelegate::paint(painter, baseOption, index);

            painter->save();
            painter->setRenderHint(QPainter::Antialiasing);

            // Determine if this specific state uses a rounded badge layout or inline text
            const bool useBadge = [] (TorrentState state) {
                switch (state)
                {
                case TorrentState::Downloading:
                case TorrentState::ForcedDownloading:
                case TorrentState::Uploading:
                case TorrentState::ForcedUploading:
                case TorrentState::StoppedDownloading:
                case TorrentState::StoppedUploading:
                case TorrentState::MissingFiles:
                case TorrentState::Error:
                    return true;
                default:
                    return false;
                }
            }(torrentState);

            if (useBadge)
            {
                // --- Badge State Rendering ---
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

                switch (torrentState)
                {
                case TorrentState::Downloading:
                case TorrentState::ForcedDownloading:
                case TorrentState::Uploading:
                case TorrentState::ForcedUploading:
                    {
                        const QColor bgColor = option.palette.color(QPalette::Active, QPalette::WindowText);
                        const QColor labelColor = option.palette.color(QPalette::Active, QPalette::Dark);
                        painter->fillPath(path, bgColor);
                        painter->setPen(labelColor);
                    }
                    break;

                case TorrentState::StoppedDownloading:
                case TorrentState::StoppedUploading:
                case TorrentState::MissingFiles:
                case TorrentState::Error:
                    {
                        const QColor bgColor = statusColor.isValid() ? statusColor : option.palette.color(QPalette::Active, QPalette::Text);
                        painter->fillPath(path, bgColor);
                        painter->setPen(Qt::white);
                    }
                    break;

                default:
                    break;
                }

                painter->drawText(badgeRect, Qt::AlignCenter, statusText);
            }
            else
            {
                // --- Non-Badge (Custom Plain Text) State Rendering ---
                // Align text consistently with your badge padding offset so they share the same X baseline
                const int paddingLeft = 5;
                QRect textRect = option.rect;
                textRect.setX(textRect.x() + paddingLeft);

                const QColor labelColor = statusColor.isValid() ? statusColor : option.palette.color(QPalette::Active, QPalette::Text);
                painter->setPen(labelColor);
                
                // Draw text explicitly via painter to match font metrics and alignment without base overlap
                painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, statusText);
            }

            painter->restore();
        }
        break;

    default:
        QStyledItemDelegate::paint(painter, option, index);
        break;
    }
}
