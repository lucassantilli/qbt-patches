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
    if (m_nameColHeight == -1)
    {
        const QModelIndex nameColumn = index.sibling(index.row(), TransferListModel::TR_NAME);
        m_nameColHeight = QStyledItemDelegate::sizeHint(option, nameColumn).height();
    }

    QModelIndex targetIndex = index;
    QString sanitizedText;

    if (index.column() == TransferListModel::TR_STATUS)
    {
        sanitizedText = index.data(Qt::DisplayRole).toString().simplified();
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
        QStyledItemDelegate::paint(painter, option, index);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        const auto torrentState = index.data(TransferListModel::UnderlyingDataRole).value<TorrentState>();
        const QString statusText = index.data(Qt::DisplayRole).toString().simplified();
        const QColor statusColor = index.data(Qt::ForegroundRole).value<QColor>();

        const int horizontalPadding = 12;
        const int paddingLeft = 5;
        const int paddingRight = 5;

        const int maxBadgeWidth = option.rect.width() - (paddingLeft + paddingRight);
        const int desiredBadgeWidth = option.fontMetrics.horizontalAdvance(statusText) + horizontalPadding;
        const int badgeWidth = std::max(0, std::min(desiredBadgeWidth, maxBadgeWidth));

        const int badgeHeight = option.rect.height() - 8;
        const int badgeX = option.rect.x() + paddingLeft;
        const int badgeY = option.rect.y() + (option.rect.height() - badgeHeight) / 2;
        const QRect badgeRect(badgeX, badgeY, badgeWidth, badgeHeight);

        const int maxTextWidth = std::max(0, badgeWidth - horizontalPadding);
        const QString elidedText = option.fontMetrics.elidedText(statusText, Qt::ElideRight, maxTextWidth);

        const int radius = 4;
        QPainterPath path;
        path.addRoundedRect(badgeRect, radius, radius);

        switch (torrentState)
        {
        case TorrentState::StalledUploading:
			{
				const bool isEvenRow = (index.row() % 2 == 0);
				const QColor bgColor = isEvenRow 
					? QColor(10, 10, 10)  // Color for even rows
					: QColor(14, 14, 14); // Color for odd rows

				const QColor textColor = QColor(229, 229, 229);

				painter->fillPath(path, bgColor);
				painter->setPen(textColor);
			}
			break;

        case TorrentState::Uploading:
		case TorrentState::ForcedUploading:
            {
                painter->fillPath(path, QColor(229, 229, 229));
                painter->setPen(QColor(10, 10, 10));
            }
            break;

        default:
            {
                const QColor bgColor = statusColor.isValid() ? statusColor : option.palette.color(QPalette::Text);
                painter->fillPath(path, bgColor);
                painter->setPen(Qt::white);
            }
            break;
        }

        painter->drawText(badgeRect, Qt::AlignCenter | Qt::TextSingleLine, elidedText);
        painter->restore();
    }
    break;

    default:
        QStyledItemDelegate::paint(painter, option, index);
        break;
    }
}
