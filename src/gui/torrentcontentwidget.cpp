#include "torrentcontentwidget.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QModelIndexList>
#include <QSet>
#include <QShortcut>
#include <QWheelEvent>
#include <QRegularExpression>

#include "base/bittorrent/torrentcontenthandler.h"
#include "base/path.h"
#include "base/utils/string.h"
#include "autoexpandabledialog.h"
#include "raisedmessagebox.h"
#include "torrentcontentfiltermodel.h"
#include "torrentcontentitemdelegate.h"
#include "torrentcontentlayoutdialog.h"
#include "torrentcontentmodel.h"
#include "torrentcontentmodelitem.h"
#include "uithememanager.h"
#include "utils.h"

#ifdef Q_OS_MACOS
#include "gui/macutilities.h"
#endif

namespace
{
    QList<QPersistentModelIndex> toPersistentIndexes(const QModelIndexList &indexes)
    {
        QList<QPersistentModelIndex> persistentIndexes;
        persistentIndexes.reserve(indexes.size());
        for (const QModelIndex &index : indexes)
            persistentIndexes.emplaceBack(index);

        return persistentIndexes;
    }

    void setCheckStateRecursively(QAbstractItemModel *model, const QModelIndex &parent, Qt::CheckState state)
    {
        const int rowCount = model->rowCount(parent);
        if (rowCount == 0)
        {
            model->setData(parent, state, Qt::CheckStateRole);
            return;
        }

        for (int i = 0; i < rowCount; ++i)
        {
            const QModelIndex index = model->index(i, TorrentContentModelItem::COL_NAME, parent);
            setCheckStateRecursively(model, index, state);
        }
    }

    void applyCheckState(QAbstractItemModel *model, const QModelIndex &index, Qt::CheckState state, bool isFilterApplied)
    {
        if (isFilterApplied)
            setCheckStateRecursively(model, index, state);
        else
            model->setData(index, state, Qt::CheckStateRole);
    }

    class CheckboxEventFilter : public QObject
    {
    public:
        CheckboxEventFilter(TorrentContentWidget *widget, TorrentContentFilterModel *filterModel)
            : QObject(widget), m_widget(widget), m_filterModel(filterModel)
        {
        }

        bool eventFilter(QObject *watched, QEvent *event) override
        {
            if (event->type() == QEvent::MouseButtonRelease)
            {
                auto *mouseEvent = static_cast<QMouseEvent *>(event);
                if (mouseEvent->button() == Qt::LeftButton)
                {
                    const QModelIndex index = m_widget->indexAt(mouseEvent->pos());
                    if (index.isValid() && (index.column() == TorrentContentModelItem::COL_NAME))
                    {
                        const bool isFilterApplied = !m_filterModel->filterRegularExpression().pattern().isEmpty();

                        // Check if a filter is active and the clicked node is a folder (has children)
                        if (isFilterApplied && (m_filterModel->rowCount(index) > 0))
                        {
                            QStyleOptionViewItem opt;
                            opt.initFrom(m_widget);
                            opt.rect = m_widget->visualRect(index);
                            opt.features |= QStyleOptionViewItem::HasCheckIndicator;

                            if (index.data(Qt::CheckStateRole).toInt() == Qt::Checked)
                                opt.state |= QStyle::State_On;
                            else
                                opt.state |= QStyle::State_Off;

                            const QRect checkRect = m_widget->style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &opt, m_widget);

                            if (checkRect.contains(mouseEvent->pos()))
                            {
                                const QVariant value = index.data(Qt::CheckStateRole);
                                if (value.isValid())
                                {
                                    const Qt::CheckState state = (static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked)
                                                                 ? Qt::Unchecked : Qt::Checked;
                                    applyCheckState(m_widget->model(), index, state, isFilterApplied);

                                    // Stop standard processing to prevent checking hidden children
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            return QObject::eventFilter(watched, event);
        }

    private:
        TorrentContentWidget *m_widget;
        TorrentContentFilterModel *m_filterModel;
    };
}

TorrentContentWidget::TorrentContentWidget(QWidget *parent)
    : QTreeView(parent)
{
    setDragDropMode(QAbstractItemView::DragOnly);
    setDragEnabled(false);
    setSelectionMode(QAbstractItemView::MultiSelection);
    setExpandsOnDoubleClick(false);
    setSortingEnabled(true);
    setUniformRowHeights(true);
    header()->setSortIndicator(0, Qt::AscendingOrder);
    header()->setFirstSectionMovable(true);
    header()->setContextMenuPolicy(Qt::CustomContextMenu);

    m_model = new TorrentContentModel(this);
    connect(m_model, &TorrentContentModel::renameFailed, this, [this](const QString &errorMessage)
    {
        RaisedMessageBox::warning(this, tr("Rename error"), errorMessage, QMessageBox::Ok);
    });

    m_filterModel = new TorrentContentFilterModel(this);
    m_filterModel->setSourceModel(m_model);
    QTreeView::setModel(m_filterModel);

    auto *itemDelegate = new TorrentContentItemDelegate(this);
    setItemDelegate(itemDelegate);

    viewport()->installEventFilter(new CheckboxEventFilter(this, m_filterModel));

    connect(this, &QAbstractItemView::clicked, this, qOverload<const QModelIndex &>(&QAbstractItemView::edit));
    connect(this, &QAbstractItemView::doubleClicked, this, &TorrentContentWidget::onItemDoubleClicked);
    connect(this, &QWidget::customContextMenuRequested, this, &TorrentContentWidget::displayContextMenu);
    connect(header(), &QWidget::customContextMenuRequested, this, &TorrentContentWidget::displayColumnHeaderMenu);
    connect(header(), &QHeaderView::sectionMoved, this, &TorrentContentWidget::stateChanged);
    connect(header(), &QHeaderView::sectionResized, this, &TorrentContentWidget::stateChanged);
    connect(header(), &QHeaderView::sortIndicatorChanged, this, &TorrentContentWidget::stateChanged);

    const auto *renameFileHotkey = new QShortcut(Qt::Key_F2, this, nullptr, nullptr, Qt::WidgetShortcut);
    connect(renameFileHotkey, &QShortcut::activated, this, &TorrentContentWidget::renameSelectedFile);

    connect(model(), &QAbstractItemModel::modelReset, this, &TorrentContentWidget::expandRecursively);
}

void TorrentContentWidget::setContentHandler(BitTorrent::TorrentContentHandler *contentHandler)
{
    m_model->setContentHandler(contentHandler);
    if (!contentHandler)
        return;
}

BitTorrent::TorrentContentHandler *TorrentContentWidget::contentHandler() const
{
    return m_model->contentHandler();
}

void TorrentContentWidget::refresh()
{
    setUpdatesEnabled(false);
    m_model->refresh();
    setUpdatesEnabled(true);
}

bool TorrentContentWidget::openByEnterKey() const
{
    return m_openFileHotkeyEnter;
}

void TorrentContentWidget::setOpenByEnterKey(const bool value)
{
    if (value == openByEnterKey())
        return;

    if (value)
    {
        m_openFileHotkeyReturn = new QShortcut(Qt::Key_Return, this, nullptr, nullptr, Qt::WidgetShortcut);
        connect(m_openFileHotkeyReturn, &QShortcut::activated, this, &TorrentContentWidget::openSelectedFile);
        m_openFileHotkeyEnter = new QShortcut(Qt::Key_Enter, this, nullptr, nullptr, Qt::WidgetShortcut);
        connect(m_openFileHotkeyEnter, &QShortcut::activated, this, &TorrentContentWidget::openSelectedFile);
    }
    else
    {
        delete m_openFileHotkeyEnter;
        m_openFileHotkeyEnter = nullptr;
        delete m_openFileHotkeyReturn;
        m_openFileHotkeyReturn = nullptr;
    }
}

TorrentContentWidget::DoubleClickAction TorrentContentWidget::doubleClickAction() const
{
    return m_doubleClickAction;
}

void TorrentContentWidget::setDoubleClickAction(DoubleClickAction action)
{
    m_doubleClickAction = action;
}

TorrentContentWidget::ColumnsVisibilityMode TorrentContentWidget::columnsVisibilityMode() const
{
    return m_columnsVisibilityMode;
}

void TorrentContentWidget::setColumnsVisibilityMode(ColumnsVisibilityMode mode)
{
    m_columnsVisibilityMode = mode;
}

int TorrentContentWidget::getFileIndex(const QModelIndex &index) const
{
    return m_filterModel->getFileIndex(index);
}

Path TorrentContentWidget::getItemPath(const QModelIndex &index) const
{
    Path path;
    for (QModelIndex i = index; i.isValid(); i = i.parent())
        path = Path(i.data().toString()) / path;
    return path;
}

void TorrentContentWidget::setFilterPattern(const QString &patternText, const FilterPatternFormat format)
{
    const QString trimmedPattern = patternText.trimmed();

    // Define video extensions list using Qt string literals
    static const QStringList videoExts = {
        u"3gp"_s, u"asf"_s, u"asx"_s, u"avi"_s, u"divx"_s, u"flv"_s, u"m2t"_s, u"m2ts"_s,
        u"m4v"_s, u"mkv"_s, u"mp4"_s, u"mpeg"_s, u"mpg"_s, u"mov"_s, u"mts"_s, u"ts"_s, u"vob"_s, u"webm"_s, u"wmv"_s
    };

    // Define picture extensions list using Qt string literals
    static const QStringList picExts = {
        u"jpg"_s, u"jpeg"_s, u"gif"_s, u"png"_s, u"bmp"_s, u"webp"_s, u"tiff"_s, u"svg"_s, u"ico"_s
    };

    auto buildExtRegex = [](const QStringList &exts, bool exclude) {
        QString joined = exts.join(u'|');
        if (exclude) {
            return QString(R"(^((?!\.(?:%1)$).)*$)").arg(joined);
        }
        return QString(R"(\.(?:%1)$)").arg(joined);
    };

    QString regexPattern;
    bool isSpecialFilter = true;

    if (trimmedPattern.compare(u"video:"_s, Qt::CaseInsensitive) == 0)
    {
        regexPattern = buildExtRegex(videoExts, false);
    }
    else if (trimmedPattern.compare(u"!video:"_s, Qt::CaseInsensitive) == 0)
    {
        regexPattern = buildExtRegex(videoExts, true);
    }
    else if (trimmedPattern.compare(u"pic:"_s, Qt::CaseInsensitive) == 0)
    {
        regexPattern = buildExtRegex(picExts, false);
    }
    else if (trimmedPattern.compare(u"!pic:"_s, Qt::CaseInsensitive) == 0)
    {
        regexPattern = buildExtRegex(picExts, true);
    }
    else if (trimmedPattern.startsWith(u"ext:"_s, Qt::CaseInsensitive))
    {
        QString ext = trimmedPattern.mid(4).trimmed();
        if (!ext.isEmpty())
            regexPattern = QString(R"(\.%1$)").arg(QRegularExpression::escape(ext));
        else
            isSpecialFilter = false;
    }
    else if (trimmedPattern.startsWith(u"!ext:"_s, Qt::CaseInsensitive))
    {
        QString ext = trimmedPattern.mid(5).trimmed();
        if (!ext.isEmpty())
            regexPattern = QString(R"(^((?!\.%1$).)*$)").arg(QRegularExpression::escape(ext));
        else
            isSpecialFilter = false;
    }
    else
    {
        isSpecialFilter = false;
    }

    if (isSpecialFilter)
    {
        m_filterModel->setFilterRegularExpression(QRegularExpression(regexPattern, QRegularExpression::CaseInsensitiveOption));
    }
    else if (format == FilterPatternFormat::PlainText)
    {
        m_filterModel->setFilterFixedString(patternText);
        m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    }
    else
    {
        const QString pattern = ((format == FilterPatternFormat::Regex)
                ? patternText : Utils::String::wildcardToRegexPattern(patternText));
        m_filterModel->setFilterRegularExpression(QRegularExpression(pattern, QRegularExpression::CaseInsensitiveOption));
    }

    if (patternText.isEmpty())
    {
        collapseAll();
        expand(m_filterModel->index(0, 0));
    }
    else
    {
        expandAll();
    }
}

void TorrentContentWidget::checkAll()
{
    const bool isFilterApplied = !m_filterModel->filterRegularExpression().pattern().isEmpty();
    for (int i = 0; i < model()->rowCount(); ++i)
        applyCheckState(model(), model()->index(i, TorrentContentModelItem::COL_NAME), Qt::Checked, isFilterApplied);
}

void TorrentContentWidget::checkNone()
{
    const bool isFilterApplied = !m_filterModel->filterRegularExpression().pattern().isEmpty();
    for (int i = 0; i < model()->rowCount(); ++i)
        applyCheckState(model(), model()->index(i, TorrentContentModelItem::COL_NAME), Qt::Unchecked, isFilterApplied);
}

void TorrentContentWidget::setContentDragAllowed(const bool allowed)
{
    m_contentDragAllowed = allowed;
}

void TorrentContentWidget::setContentDragEnabled(const bool enabled)
{
    m_contentDragEnabled = enabled;
}

void TorrentContentWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_contentDragAllowed)
    {
        const bool hasAlt = event->modifiers().testFlag(Qt::AltModifier);
        setDragEnabled(hasAlt ? !m_contentDragEnabled : m_contentDragEnabled);
    }

    QTreeView::mousePressEvent(event);
}

void TorrentContentWidget::keyPressEvent(QKeyEvent *event)
{
    if ((event->key() != Qt::Key_Space) && (event->key() != Qt::Key_Select))
    {
        QTreeView::keyPressEvent(event);
        return;
    }

    event->accept();

    const QVariant value = currentNameCell().data(Qt::CheckStateRole);
    if (!value.isValid())
    {
        Q_ASSERT(false);
        return;
    }

    const Qt::CheckState state = (static_cast<Qt::CheckState>(value.toInt()) == Qt::Checked)
                                 ? Qt::Unchecked : Qt::Checked;
    const QList<QPersistentModelIndex> selection = toPersistentIndexes(selectionModel()->selectedRows(TorrentContentModelItem::COL_NAME));

    const bool isFilterApplied = !m_filterModel->filterRegularExpression().pattern().isEmpty();
    for (const QPersistentModelIndex &index : selection)
        applyCheckState(model(), index, state, isFilterApplied);
}

void TorrentContentWidget::renameSelectedFile()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1)
        return;

    const QPersistentModelIndex modelIndex = selectedIndexes.first();
    if (!modelIndex.isValid())
        return;

    // Ask for new name
    const bool isFile = (m_filterModel->itemType(modelIndex) == TorrentContentModelItem::FileType);
    bool ok = false;
    QString newName = AutoExpandableDialog::getText(this, tr("Renaming"), tr("New name:"), QLineEdit::Normal
            , modelIndex.data().toString(), &ok, isFile).trimmed();
    if (!ok || !modelIndex.isValid())
        return;

    model()->setData(modelIndex, newName);
}

void TorrentContentWidget::batchRenameFiles()
{
    const QModelIndexList selectedRows = selectionModel()->selectedRows();
    QSet<int> fileIndexes;
    for (const QModelIndex &rowIndex : selectedRows)
        fileIndexes.unite(m_filterModel->getFileIndexes(rowIndex));

    auto *dialog = new TorrentContentLayoutDialog(m_model->contentHandler(), fileIndexes, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    connect(m_model->contentHandler(), &QObject::destroyed, dialog, &QDialog::reject);
    dialog->open();
}

void TorrentContentWidget::applyPriorities(const BitTorrent::DownloadPriority priority)
{
    const QList<QPersistentModelIndex> selectedRows = toPersistentIndexes(selectionModel()->selectedRows(Priority));
    for (const QPersistentModelIndex &index : selectedRows)
    {
        model()->setData(index, static_cast<int>(priority));
    }
}

void TorrentContentWidget::applyPrioritiesByOrder()
{
    // Equally distribute the selected items into groups and for each group assign
    // a download priority that will apply to each item. The number of groups depends on how
    // many "download priority" are available to be assigned

    const QList<QPersistentModelIndex> selectedRows = toPersistentIndexes(selectionModel()->selectedRows(Priority));

    const qsizetype priorityGroups = 3;
    const auto priorityGroupSize = std::max<qsizetype>((selectedRows.length() / priorityGroups), 1);

    for (qsizetype i = 0; i < selectedRows.length(); ++i)
    {
        auto priority = BitTorrent::DownloadPriority::Ignored;
        switch (i / priorityGroupSize)
        {
        case 0:
            priority = BitTorrent::DownloadPriority::Maximum;
            break;
        case 1:
            priority = BitTorrent::DownloadPriority::High;
            break;
        default:
        case 2:
            priority = BitTorrent::DownloadPriority::Normal;
            break;
        }

        const QPersistentModelIndex &index = selectedRows[i];
        model()->setData(index, static_cast<int>(priority));
    }
}

void TorrentContentWidget::openSelectedFile()
{
    const QModelIndexList selectedIndexes = selectionModel()->selectedRows(0);
    if (selectedIndexes.size() != 1)
        return;
    openItem(selectedIndexes.first());
}

void TorrentContentWidget::setModel([[maybe_unused]] QAbstractItemModel *model)
{
    Q_ASSERT_X(false, Q_FUNC_INFO, "Changing the model of TorrentContentWidget is not allowed.");
}

QModelIndex TorrentContentWidget::currentNameCell() const
{
    const QModelIndex current = currentIndex();
    if (!current.isValid())
    {
        Q_ASSERT(false);
        return {};
    }

    return current.siblingAtColumn(TorrentContentModelItem::COL_NAME);
}

void TorrentContentWidget::displayColumnHeaderMenu()
{
    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setToolTipsVisible(true);

    if (m_columnsVisibilityMode == ColumnsVisibilityMode::Editable)
    {
        menu->setTitle(tr("Column visibility"));
        for (int i = 0; i < TorrentContentModelItem::NB_COL; ++i)
        {
            const auto columnName = model()->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
            QAction *action = menu->addAction(columnName, this, [this, i](bool checked)
            {
                setColumnHidden(i, !checked);

                if (checked && (columnWidth(i) <= 5))
                    resizeColumnToContents(i);

                emit stateChanged();
            });
            action->setCheckable(true);
            action->setChecked(!isColumnHidden(i));

            if (i == TorrentContentModelItem::COL_NAME)
                action->setEnabled(false);
        }

        menu->addSeparator();
    }

    QAction *resizeAction = menu->addAction(tr("Resize columns"), this, [this]()
    {
        for (int i = 0, count = header()->count(); i < count; ++i)
        {
            if (!isColumnHidden(i))
                resizeColumnToContents(i);
        }

        emit stateChanged();
    });
    resizeAction->setToolTip(tr("Resize all non-hidden columns to the size of their contents"));

    menu->popup(QCursor::pos());
}

void TorrentContentWidget::displayContextMenu()
{
    const QModelIndexList selectedRows = selectionModel()->selectedRows(0);
    if (selectedRows.empty())
        return;

    QMenu *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    if (selectedRows.size() == 1)
    {
        const QModelIndex index = selectedRows[0];

        if (!contentHandler()->actualStorageLocation().isEmpty())
        {
            menu->addAction(UIThemeManager::instance()->getIcon(u"folder-documents"_s), tr("Open")
                    , this, [this, index]() { openItem(index); });
            menu->addAction(UIThemeManager::instance()->getIcon(u"directory"_s), tr("Open containing folder")
                    , this, [this, index]() { openParentFolder(index); });
            menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("Copy path")
                    , this, [this, index]() { copyFullPath(index); });
        }
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s), tr("Rename...")
                , this, &TorrentContentWidget::renameSelectedFile);
    }

    menu->addAction(UIThemeManager::instance()->getIcon(u"edit-rename"_s), tr("Batch rename...")
            , this, &TorrentContentWidget::batchRenameFiles);
    menu->addSeparator();

    QMenu *subMenu = menu->addMenu(tr("Priority"));
    subMenu->addAction(tr("Do not download"), this, [this]
    {
        applyPriorities(BitTorrent::DownloadPriority::Ignored);
    });
    subMenu->addAction(tr("Normal"), this, [this]
    {
        applyPriorities(BitTorrent::DownloadPriority::Normal);
    });
    subMenu->addAction(tr("High"), this, [this]
    {
        applyPriorities(BitTorrent::DownloadPriority::High);
    });
    subMenu->addAction(tr("Maximum"), this, [this]
    {
        applyPriorities(BitTorrent::DownloadPriority::Maximum);
    });
    subMenu->addSeparator();
    subMenu->addAction(tr("By shown file order"), this, &TorrentContentWidget::applyPrioritiesByOrder);

    // The selected torrent might have disappeared during exec()
    // so we just close menu when an appropriate model is reset
    connect(model(), &QAbstractItemModel::modelAboutToBeReset, menu, [menu]()
    {
        menu->setActiveAction(nullptr);
        menu->close();
    });

    menu->popup(QCursor::pos());
}

void TorrentContentWidget::openItem(const QModelIndex &index) const
{
    if (!index.isValid())
        return;

    m_model->contentHandler()->flushCache();  // Flush data
    Utils::Gui::openPath(getFullPath(index));
}

void TorrentContentWidget::openParentFolder(const QModelIndex &index)
{
    const Path path = getFullPath(index);
    m_model->contentHandler()->flushCache();  // Flush data
#ifdef Q_OS_MACOS
    MacUtils::openFiles({path});
#else
    Utils::Gui::openFolderSelect(path);
#endif
}

void TorrentContentWidget::copyFullPath(const QModelIndex &index)
{
    const Path path = getFullPath(index);
    QApplication::clipboard()->setText(path.toString());
}

Path TorrentContentWidget::getFullPath(const QModelIndex &index) const
{
    const auto *contentHandler = m_model->contentHandler();
    if (const int fileIdx = getFileIndex(index); fileIdx >= 0)
    {
        const Path fullPath = contentHandler->actualStorageLocation() / contentHandler->actualFilePath(fileIdx);
        return fullPath;
    }

    // folder type
    const Path fullPath = contentHandler->actualStorageLocation() / getItemPath(index);
    return fullPath;
}

void TorrentContentWidget::onItemDoubleClicked(const QModelIndex &index)
{
    const auto *contentHandler = m_model->contentHandler();
    Q_ASSERT(contentHandler && contentHandler->hasMetadata());

    if (!contentHandler || !contentHandler->hasMetadata()) [[unlikely]]
        return;

    if (m_doubleClickAction == DoubleClickAction::Rename)
        renameSelectedFile();
    else
        openItem(index);
}

void TorrentContentWidget::expandRecursively()
{
    QModelIndex currentIndex;
    while (model()->rowCount(currentIndex) == 1)
    {
        currentIndex = model()->index(0, 0, currentIndex);
        setExpanded(currentIndex, true);
    }
}

void TorrentContentWidget::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier)
    {
        // Shift + scroll = horizontal scroll
        event->accept();
        QWheelEvent scrollHEvent {event->position(), event->globalPosition()
                    , event->pixelDelta(), event->angleDelta().transposed(), event->buttons()
                    , event->modifiers(), event->phase(), event->inverted(), event->source()};
        QTreeView::wheelEvent(&scrollHEvent);
        return;
    }

    QTreeView::wheelEvent(event);  // event delegated to base class
}

void TorrentContentWidget::rowsInserted(const QModelIndex &parent, int start, int end)
{
    QTreeView::rowsInserted(parent, start, end);

    // Expand all parents if the parent(s) of the node are not expanded.
    QModelIndex p = parent;
    while (p.isValid())
    {
        if (!isExpanded(p))
            expand(p);
        p = model()->parent(p);
    }
}
