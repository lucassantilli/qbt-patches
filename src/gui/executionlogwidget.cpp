#include "executionlogwidget.h"

#include <QDateTime>
#include <QMenu>
#include <QPalette>
#include <QTimer>

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

    QList<QPushButton*> filterButtons = {
        m_ui->checkNormal, m_ui->checkInfo, m_ui->checkWarning,
        m_ui->checkCritical, m_ui->checkRSS, m_ui->checkPeer
    };

    bool updatingFilters = false;

    auto updateFilterFromUI = [this, filterButtons, &updatingFilters]() {
        if (updatingFilters) return;

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

        // Rule 2: Disabling all filters automatically enables all filters
        if (activeTypes == 0) {
            updatingFilters = true;
            for (QPushButton *btn : filterButtons) {
                btn->setChecked(true);
            }
            updatingFilters = false;
            
            activeTypes = Log::NORMAL | Log::INFO | Log::WARNING | Log::CRITICAL | Log::RSS | Log::PEER;
        }

        m_messageFilterModel->setMessageTypes(activeTypes);
    };

    // Setup press-and-hold timers for each button
    for (QPushButton *btn : filterButtons) {
        QTimer *holdTimer = new QTimer(this);
        holdTimer->setSingleShot(true);
        holdTimer->setInterval(1000); // 1 second

        connect(btn, &QPushButton::pressed, this, [holdTimer]() {
            holdTimer->start();
        });

        connect(btn, &QPushButton::released, this, [holdTimer]() {
            holdTimer->stop();
        });

        // Rule 1: Press and hold any filter button for 1 second disables all other filters
        connect(holdTimer, &QTimer::timeout, this, [this, filterButtons, btn, &updatingFilters]() {
            updatingFilters = true;
            for (QPushButton *otherBtn : filterButtons) {
                otherBtn->setChecked(otherBtn == btn);
            }
            updatingFilters = false;

            updateFilterFromUI();
        });

        connect(btn, &QPushButton::toggled, this, updateFilterFromUI);
    }
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
