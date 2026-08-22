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

    // Sync button check states using the incoming 'types' parameter
    m_ui->checkNormal->setChecked(types.testFlag(Log::NORMAL));
    m_ui->checkInfo->setChecked(types.testFlag(Log::INFO));
    m_ui->checkWarning->setChecked(types.testFlag(Log::WARNING));
    m_ui->checkCritical->setChecked(types.testFlag(Log::CRITICAL));
    m_ui->checkRSS->setChecked(types.testFlag(Log::RSS));
    m_ui->checkPeer->setChecked(types.testFlag(Log::PEER));

    LogMessageModel *messageModel = new LogMessageModel(this);
    m_messageFilterModel->setSourceModel(messageModel);

    LogListView *messageView = new LogListView(this);
    messageView->setModel(m_messageFilterModel);
    messageView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(messageView, &LogListView::customContextMenuRequested, this, [this, messageView, messageModel]()
    {
        displayContextMenu(messageView, messageModel);
    });

    m_ui->logViewLayout->addWidget(messageView);

    auto updateFilterFromUI = [this]() {
        Log::MsgTypes activeTypes = {};
        if (m_ui->checkNormal->isChecked())
            activeTypes |= Log::NORMAL;
        if (m_ui->checkInfo->isChecked())
            activeTypes |= Log::INFO;
        if (m_ui->checkWarning->isChecked())
            activeTypes |= Log::WARNING;
        if (m_ui->checkCritical->isChecked())
            activeTypes |= Log::CRITICAL;
        if (m_ui->checkRSS->isChecked())
            activeTypes |= Log::RSS;
        if (m_ui->checkPeer->isChecked())
            activeTypes |= Log::PEER;

        m_messageFilterModel->setMessageTypes(activeTypes);
    };

    connect(m_ui->checkNormal, &QPushButton::toggled, this, updateFilterFromUI);
    connect(m_ui->checkInfo, &QPushButton::toggled, this, updateFilterFromUI);
    connect(m_ui->checkWarning, &QPushButton::toggled, this, updateFilterFromUI);
    connect(m_ui->checkCritical, &QPushButton::toggled, this, updateFilterFromUI);
    connect(m_ui->checkRSS, &QPushButton::toggled, this, updateFilterFromUI);
    connect(m_ui->checkPeer, &QPushButton::toggled, this, updateFilterFromUI);
}

ExecutionLogWidget::~ExecutionLogWidget()
{
    delete m_ui;
}

void ExecutionLogWidget::setMessageTypes(const Log::MsgTypes types)
{
    m_messageFilterModel->setMessageTypes(types);
    
    // Keep UI checkboxes in sync if changed externally (e.g. from MainWindow menu)
    m_ui->checkNormal->setChecked(types.testFlag(Log::NORMAL));
    m_ui->checkInfo->setChecked(types.testFlag(Log::INFO));
    m_ui->checkWarning->setChecked(types.testFlag(Log::WARNING));
    m_ui->checkCritical->setChecked(types.testFlag(Log::CRITICAL));
    m_ui->checkRSS->setChecked(types.testFlag(Log::RSS));
    m_ui->checkPeer->setChecked(types.testFlag(Log::PEER));
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
