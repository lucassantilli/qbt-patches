#include "executionlogwidget.h"

#include <QDateTime>
#include <QMenu>
#include <QPalette>

#include "base/global.h"
#include "log/logfiltermodel.h"
#include "log/loglistview.h"
#include "log/logmodel.h"
#include "ui_executionlogwidget.h"
#include "uithememanager.h"

ExecutionLogWidget::ExecutionLogWidget(const Log::MsgTypes types, QWidget *parent)
    : QWidget(parent)
    , m_ui(new Ui::ExecutionLogWidget)
    , m_messageFilterModel(new LogFilterModel(types, this))
{
    m_ui->setupUi(this);

    // Initialize the source model for logs
    LogMessageModel *messageModel = new LogMessageModel(this);
    m_messageFilterModel->setSourceModel(messageModel);

    LogListView *messageView = new LogListView(this);
    messageView->setModel(m_messageFilterModel);
    messageView->setContextMenuPolicy(Qt::CustomContextMenu);
    
    connect(messageView, &LogListView::customContextMenuRequested, this, [this, messageView, messageModel]()
    {
        displayContextMenu(messageView, messageModel);
    });

    // Embed the unified log view into the layout below the 50px filter bar
    m_ui->logViewLayout->addWidget(messageView);

    // Connect top filter buttons to dynamically update the bitmask filter model
    auto updateFilterFromUI = [this]() {
        Log::MsgTypes activeTypes = {};
        if (m_ui->checkNormal->isChecked())
            activeTypes |= Log::Normal;
        if (m_ui->checkWarning->isChecked())
            activeTypes |= Log::Warning;
        if (m_ui->checkCritical->isChecked())
            activeTypes |= Log::Critical;
        if (m_ui->checkPeer->isChecked())
            activeTypes |= Log::Peer;

        m_messageFilterModel->setMessageTypes(activeTypes);
    };

    connect(m_ui->checkNormal, &QPushButton::toggled, this, updateFilterFromUI);
    connect(m_ui->checkWarning, &QPushButton::toggled, this, updateFilterFromUI);
    connect(m_ui->checkCritical, &QPushButton::toggled, this, updateFilterFromUI);
    connect(m_ui->checkPeer, &QPushButton::toggled, this, updateFilterFromUI);

    // Set initial filter state based on default button checks
    updateFilterFromUI();
}

ExecutionLogWidget::~ExecutionLogWidget()
{
    delete m_ui;
}

void ExecutionLogWidget::setMessageTypes(const Log::MsgTypes types)
{
    m_messageFilterModel->setMessageTypes(types);
}

void ExecutionLogWidget::displayContextMenu(const LogListView *view, const BaseLogModel *model) const
{
    QMenu *menu = new QMenu;
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // only show copy action if any of the row is selected
    if (view->currentIndex().isValid())
    {
        menu->addAction(UIThemeManager::instance()->getIcon(u"edit-copy"_s), tr("Copy")
            , view, &LogListView::copySelection);
    }

    menu->addAction(UIThemeManager::instance()->getIcon(u"edit-clear"_s), tr("Clear")
        , model, &BaseLogModel::reset);

    menu->popup(QCursor::pos());
}
