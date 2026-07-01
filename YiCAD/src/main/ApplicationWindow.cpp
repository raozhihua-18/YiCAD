/*
 * Copyright (C) 2024-2026 YiCAD Contributors
 *
 * This file is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 */

/// @file ApplicationWindow.cpp
/// @brief 应用程序主窗口实现，管理Ribbon界面、绘图区域、图层面板和事件分发

#include "ApplicationWindow.h"
#include <QFile>
#include <QTextEdit>
#include <QAbstractButton>
#include <QFileDialog>
#include "SARibbonBar.h"
#include "SARibbonCategory.h"
#include <QPushButton>
#include <QMessageBox>
#include "SARibbonPannel.h"
#include "SARibbonToolButton.h"
#include <QAction>
#include <QMenu>
#include <QStatusBar>
#include <QDebug>
#include <QElapsedTimer>
#include <QRadioButton>
#include <QButtonGroup>
#include <QWidgetAction>
#include "SARibbonMenu.h"
#include "SARibbonComboBox.h"
#include "SARibbonLineEdit.h"
#include "SARibbonGallery.h"
#include "SARibbonCheckBox.h"
#include "SARibbonQuickAccessBar.h"
#include "SARibbonButtonGroupWidget.h"
#include "SARibbonApplicationButton.h"
#include "SARibbonCustomizeWidget.h"
#include "SAWindowButtonGroup.h"
#include <QCalendarWidget>
#include "SARibbonCustomizeDialog.h"
#include <QXmlStreamWriter>
#include <QTextStream>
#include <QFontComboBox>
#include <QLabel>
#include "SAFramelessHelper.h"
#include "CustomComboboxItem.h"
#include <QListWidget>
#include <QMdiArea>
#include <QString>
#include <QApplication>
#include <QToolBar>
#include <QDockWidget>
#include <QToolButton>
#include <QTabWidget>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QSettings>
#include <QVariant>

#include "UITabDrawWidget.h"
#include "UIBottomWidget.h"
#include "UIActionHandler.h"
#include "UICommandWidget.h"
#include "UIBlockListWidget.h"
#include "UIBlockSaveAs.h"
#include "UIDialogFactory.h"
#include "UICurrentActivePen.h"
#include "AIAssistant.h"

#include "ActionLayersActivate.h"
#include "ActionLayersFreeze.h"
#include "ActionLayersLock.h"
#include "ActionLayersPrint.h"
#include "ActionLayersColor.h"
#include "ActionLayersDelete.h"

#include "MDIWindow.h"
#include "GuiDocumentView.h"
#include "DmSettings.h"
#include "DmDocument.h"
#include "DmLayer.h"
#include "DmColor.h"
#include "DmPen.h"
#include "GuiDialogFactory.h"
#include "UIDialogFactory.h"
#include "Selection.h"
#include "DmSystem.h"
#include "Debug.h"
#include "GuiDialogFactory.h"
#include "GuiEventHandler.h"
#include "Commands.h"
#include "DmFontList.h"

#include "HostApi.h"
#include "PluginManager.h"
#include "PluginRegistry.h"
#include "PluginUiAdapter.h"

#include "DmLine.h"
#include "DmCircle.h"
#include "DmArc.h"
#include "DmSolid.h"
#include "DmPoint.h"
#include "DmEllipse.h"
#include "DmRay.h"
#include "DmXline.h"
#include "DmSpline.h"

/// @brief 将插件宿主服务限制在主窗口公开的文档和消息接口内。
class ApplicationPluginHostContext final : public PluginHostContext
{
public:
    explicit ApplicationPluginHostContext(ApplicationWindow& window) noexcept
        : m_window(window)
    {
    }

    void showPluginMessage(const QString& message) override
    {
        auto* commandWidget = m_window.getCmdWidget();
        if (commandWidget != nullptr && m_window.getMDIWindow() != nullptr)
        {
            commandWidget->appCmdTempText(message);
            return;
        }
        m_window.statusBar()->showMessage(message, 5000);
    }

    DmDocument* currentDocument() const noexcept override
    {
        return m_window.getDocument();
    }

    bool isDocumentOpen(const DmDocument* document) const noexcept override
    {
        const auto* tabs = m_window.getTabDrawWidget();
        return document != nullptr && tabs != nullptr &&
               tabs->getTabDrawDataOfDocument(document) != nullptr;
    }

    GuiDocumentView* documentView(
        const DmDocument* document) const noexcept override
    {
        auto* tabs = m_window.getTabDrawWidget();
        auto* tab = tabs == nullptr
            ? nullptr
            : tabs->getTabDrawDataOfDocument(document);
        return tab == nullptr || tab->mdiWindow == nullptr
            ? nullptr
            : tab->mdiWindow->getDocumentView();
    }

private:
    ApplicationWindow& m_window;
};

// TODO: 以下宏为性能调试用函数式宏，无法直接转换为constexpr，建议后续改为内联函数
#define PRINT_COST_START()                                                                                             \
    QElapsedTimer __TMP_COST;                                                                                          \
    int __TMP_LASTTIMES = 0

#define PRINT_COST(STR)                                                                                                \
    do {                                                                                                               \
        int ___TMP_INT = __TMP_COST.elapsed();                                                                         \
        qDebug() << STR << " cost " << ___TMP_INT - __TMP_LASTTIMES << " ms (" << ___TMP_INT << ")";                   \
        __TMP_LASTTIMES = ___TMP_INT;                                                                                  \
    } while (0)

namespace
{
constexpr int kRibbonButtonGroupMargin = 5;
constexpr int kRibbonButtonGroupSpacing = 5;

void applyLightThemeStyle(QWidget* rootWidget)
{
	QFile styleFile(QStringLiteral(":/styles/light.qss"));
	if (!styleFile.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		return;
	}

	const QString styleSheet = QString::fromUtf8(styleFile.readAll());
	qApp->setStyleSheet(styleSheet);
	if (rootWidget)
	{
		rootWidget->setStyleSheet(rootWidget->styleSheet() + QStringLiteral("\n") + styleSheet);
	}
}

SARibbonButtonGroupWidget* createRibbonButtonGroup(QWidget* parent, int rowNumber = 2)
{
	SARibbonButtonGroupWidget* group = new SARibbonButtonGroupWidget(parent);
	if (parent && parent->inherits("SARibbonPannel"))
	{
		delete group->layout();
		QGridLayout* layout = new QGridLayout(group);
		layout->setDefaultPositioning(rowNumber, Qt::Vertical);
		layout->setContentsMargins(
			kRibbonButtonGroupMargin,
			kRibbonButtonGroupMargin,
			kRibbonButtonGroupMargin,
			kRibbonButtonGroupMargin);
		layout->setSpacing(kRibbonButtonGroupSpacing);
		group->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
	}
	return group;
}
}

ApplicationWindow* ApplicationWindow::appWindow = nullptr;

ApplicationWindow::ApplicationWindow(QWidget* par)
	: SARibbonMainWindow(par)
	, m_customizeWidget(nullptr)
	, m_pActionHandler(new UIActionHandler(this))
	, m_pLayerTable(nullptr)
	, m_pLayerTableWidget(nullptr)
	, m_pCurrentLayerItem(new ComboBoxData())
	, m_pLibraryList(nullptr)
{
	appWindow = this;
	PRINT_COST_START();
	SAFramelessHelper* helper = framelessHelper();
	helper->setRubberBandOnResize(false);
	setWindowTitle("YiCAD");
	// 底部状态栏
	m_pBottomWidget = new UIBottomWindow(this);

	// 添加绘图区域底板
	m_pDrawingArea = new QMdiArea(this);
	m_pDrawingArea->move(0, 185);
	m_pDrawingArea->setAutoFillBackground(true);

	// actions过程中弹出的配置框
	QWidget* pDialogBackWidget = new QWidget(this);
	pDialogBackWidget->move(1, 212);
	pDialogBackWidget->setObjectName("actionOptionsBackWidget");

	m_pDialogFactory = new UIDialogFactory(this, pDialogBackWidget);
	GuiDialogFactory::instance()->setFactoryObject(m_pDialogFactory);
	m_pDialogFactory->setActionHandle(m_pActionHandler);
	pDialogBackWidget->hide();

	// 绘图区域选项卡
	m_pTabDrawWidget = new UITabDrawWidget(this);
	m_pTabDrawWidget->createTabDrawWidget(m_pDrawingArea, m_pActionHandler, m_pBottomWidget);
	// 获取当前绘图画布
	m_pCurrentMdiWin = m_pTabDrawWidget->getCurrentMdiWindow();

	// ===========================ActionHandler参数设置======================
	m_pActionHandler->setMdiArea(m_pDrawingArea);
	m_pActionHandler->setMDIWindow(m_pCurrentMdiWin);
	m_pActionHandler->setUITabDrawWidget(m_pTabDrawWidget);
	
	// 底部状态栏
	m_pBottomWidget->createBottomWindow(m_pActionHandler, m_pCurrentMdiWin);
	UIDIALOGFACTORY->setBottomWidget(m_pBottomWidget);
	m_pActionHandler->setSnapToolBar(m_pBottomWidget->getSnapToolBar());

	// Command窗口
	m_cmdWin = new UICommandWidget(this, m_pTabDrawWidget);
	m_cmdWin->setActionHandler(m_pActionHandler);
	m_editLine = m_cmdWin->createTempEdit();
	// 将事件和状态链接起来
	UIDIALOGFACTORY->setCommandWidget(m_cmdWin);
	m_cmdWin->getCommandWidget()->setFixedWidth(400);
	m_cmdWin->getInfoWidget()->resize(400, 100);

	// 选项卡切换事件
	connect(m_pTabDrawWidget, SIGNAL(tabChangeSignals()), this, SLOT(slotsTabChangeEvent()));

	// 基类
	m_pRibbon = ribbonBar();
	applyLightThemeStyle(this);
	PRINT_COST("setCentralWidget & setWindowTitle");
	m_pRibbon->applicationButton()->hide();            // 隐藏父标签
	
	// 主窗口导航条
	SARibbonQuickAccessBar* quickAccessBar = m_pRibbon->quickAccessBar();
	createQuickAccessBar(quickAccessBar);
	PRINT_COST("add quick access bar");

	//添加文件标签页 - 通过addCategoryPage工厂函数添加
	SARibbonCategory* categoryFile = m_pRibbon->addCategoryPage(QObject::tr("File"));
	categoryFile->setObjectName("categoryFile");
	createCategoryFile(categoryFile);
	PRINT_COST("new file page");

	//添加绘图标签页
	SARibbonCategory* categoryDraw2d = new SARibbonCategory();
	categoryDraw2d->setCategoryName(QObject::tr("Draw2d"));
	categoryDraw2d->setObjectName("categoryDraw2d");
	m_pRibbon->addCategoryPage(categoryDraw2d);
	createCategoryDraw2d(categoryDraw2d);
	auto iDraw2d = ribbonBar()->categoryIndex(categoryDraw2d) ;
	PRINT_COST("add category draw2d page");

	//添加参数化
	/*SARibbonCategory* categorySolver = new SARibbonCategory();
	categorySolver->setCategoryName(QObject::tr("Solver"));
	categorySolver->setObjectName("categorySolver");
	m_pRibbon->addCategoryPage(categorySolver);
	createCategorySolver(categorySolver);
	PRINT_COST("add category solver page");*/


	////添加视图标签页
	//SARibbonCategory* categoryViewport = new SARibbonCategory();
	//categoryViewport->setCategoryName(QObject::tr("Viewport"));
	//categoryViewport->setObjectName("categoryViewport");
	//createCategoryViewport(categoryViewport);
	//m_pRibbon->addCategoryPage(categoryViewport);
	//PRINT_COST("add viewport page");

	//添加设置标签页 - 直接new SARibbonCategory添加
	SARibbonCategory* categoryOptions = new SARibbonCategory();
	categoryOptions->setCategoryName(QObject::tr("Options"));
	categoryOptions->setObjectName("categoryOptions");
	createCategoryOptions(categoryOptions);
	m_pRibbon->addCategoryPage(categoryOptions);
	PRINT_COST("add options page");

	//// 帮助
	//SARibbonCategory* categoryHelp = new SARibbonCategory();
	//categoryHelp->setCategoryName("Help");
	//categoryHelp->setObjectName("categoryHelp");
	//ribbon->addCategoryPage(categoryHelp);
	//PRINT_COST("add category help page");

	// 主窗体最小尺寸
	this->setMinimumSize(900, 700);
	showMaximized();

	//Draw2d 作为缺省激活Tab 
	ribbonBar()->setCurrentIndex(iDraw2d);

	GuiDocumentView* view = m_pCurrentMdiWin->getDocumentView();

	// ===========================AI 助手按钮=========================
	SARibbonButtonGroupWidget* rightGroup = m_pRibbon->rightButtonGroup();
	if (rightGroup)
	{
		m_pActAI = createAction(tr("AI Assistant"), ":/ribbon/tabbar/ai.svg", "ai-assistant");
		m_pAIAssistant = new AIAssistant(this, this);
		connect(m_pActAI, &QAction::triggered, this, [this]() {
			DmDocument* doc = nullptr;
			GuiDocumentView* docView = nullptr;
			if (m_pCurrentMdiWin)
			{
				doc = m_pCurrentMdiWin->getDocument();
				docView = m_pCurrentMdiWin->getDocumentView();
			}
			m_pAIAssistant->show(doc, docView);
		});
		rightGroup->addAction(m_pActAI);
	}

    /// @brief Ribbon、命令窗口和首个文档就绪后，接入唯一的新插件加载路径。
    m_pluginHostContext =
        std::make_unique<ApplicationPluginHostContext>(*this);
    m_pluginRegistry = std::make_unique<PluginRegistry>();
    m_pluginHostApi = std::make_unique<HostApi>(
        *m_pluginHostContext, *m_pluginRegistry);
    m_pluginManager = std::make_unique<PluginManager>(
        *m_pluginHostApi, *m_pluginRegistry);
    m_pluginManager->loadAll();

    m_pluginUiAdapter = std::make_unique<PluginUiAdapter>(
        *m_pRibbon, *m_pluginRegistry, *m_pActionHandler, *m_cmdWin);
    m_pluginUiAdapter->materialize();
}


void ApplicationWindow::onStyleClicked(int id)
{
	ribbonBar()->setRibbonStyle(static_cast<SARibbonBar::RibbonStyle>(id));
}

void ApplicationWindow::onActionCustomizeTriggered(bool b)
{
	Q_UNUSED(b);
	if (nullptr == m_customizeWidget)
	{
		m_customizeWidget = new SARibbonCustomizeWidget(this, this, Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint | Qt::Dialog);
		m_customizeWidget->setWindowModality(Qt::ApplicationModal);  //设置阻塞类型
		m_customizeWidget->setAttribute(Qt::WA_ShowModal, true);     //属性设置 true:模态 false:非模态
		m_customizeWidget->setupActionsManager(m_actMgr);
	}
	m_customizeWidget->show();
	m_customizeWidget->applys();
}

void ApplicationWindow::onActionCustomizeAndSaveTriggered(bool b)
{
	Q_UNUSED(b);
	SARibbonCustomizeDialog dlg(this);
	dlg.setupActionsManager(m_actMgr);
	dlg.fromXml("customize.xml");
	if (SARibbonCustomizeDialog::Accepted == dlg.exec())
	{
		dlg.applys();
		QByteArray str;
		QXmlStreamWriter xml(&str);
		xml.setAutoFormatting(true);
		xml.setAutoFormattingIndent(2);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)  // QXmlStreamWriter always encodes XML in UTF-8.
		xml.setCodec("utf-8");
#endif
		xml.writeStartDocument();
		bool isok = dlg.toXml(&xml);
		xml.writeEndDocument();
		if (isok)
		{
			QFile f("customize.xml");
			if (f.open(QIODevice::ReadWrite | QIODevice::Text | QIODevice::Truncate))
			{
				QTextStream s(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)  // QTextStream always encodes XML in UTF-8.
				s.setCodec("utf-8");
#endif
				s << str;
				s.flush();
			}
		}
	}
}

void ApplicationWindow::onActionRemoveAppBtnTriggered(bool b)
{
	if (b)
	{
		ribbonBar()->setApplicationButton(nullptr);
	}
	else
	{
		SARibbonApplicationButton* actionRemoveAppBtn = new SARibbonApplicationButton();
		actionRemoveAppBtn->setText(QObject::tr("File"));
		this->ribbonBar()->setApplicationButton(actionRemoveAppBtn);
	}
}

void ApplicationWindow::onActionUseQssTriggered()
{
	QFile f("ribbon.qss");
	if (!f.exists())
	{
		QString fdir = QFileDialog::getOpenFileName(this, QObject::tr("select qss file"));
		if (fdir.isEmpty())
		{
			return;
		}
		f.setFileName(fdir);
	}
	if (!f.open(QIODevice::ReadWrite))
	{
		return;
	}
	QString qss(f.readAll());
	this->ribbonBar()->setStyleSheet(qss);
}

void ApplicationWindow::onActionLoadCustomizeXmlFileTriggered()
{
	//只能调用一次
	static bool has_call = false;
	if (!has_call)
	{
		has_call = sa_apply_customize_from_xml_file("customize.xml", this, m_actMgr);
	}
}

void ApplicationWindow::onActionWindowFlagNormalButtonTriggered(bool b)
{
	if (b)
	{
		//最大最小关闭按钮都有
		Qt::WindowFlags f = windowFlags();
		f |= (Qt::WindowMinMaxButtonsHint | Qt::WindowCloseButtonHint);
		updateWindowFlag(f);
	}
	else
	{
		//由于已经处于frameless状态，这个最大最小设置是无效的
		// setWindowFlags(windowFlags()&~Qt::WindowMaximizeButtonHint&~Qt::WindowMinimizeButtonHint);
		Qt::WindowFlags f = windowFlags();
		f &= ~Qt::WindowMinMaxButtonsHint & ~Qt::WindowCloseButtonHint;
		updateWindowFlag(f);
	}
}

void ApplicationWindow::closeEvent(QCloseEvent* event)
{
	m_pTabDrawWidget->slotFileCloseAll();
	if (m_pTabDrawWidget->getTabDrawList()->size() > 0)
	{
		event->ignore();
		return;
	}
	close();
}

void ApplicationWindow::keyPressEvent(QKeyEvent* e)
{
	if (e->matches(QKeySequence::Cut))
	{
		m_pActionHandler->slotEditCut();
	}
	else if (e->matches(QKeySequence::Copy))
	{
		m_pActionHandler->slotEditCopy();
	}
	else if (e->matches(QKeySequence::Paste))
	{
		m_pActionHandler->slotEditPaste();
	}
	else if (e->matches(QKeySequence::New))
	{
		m_pActionHandler->slotFileNew();
	}
	else if (e->matches(QKeySequence::Open))
	{
		m_pActionHandler->slotFileOpen();
	}
	else if (e->matches(QKeySequence::Save))
	{
		m_pActionHandler->slotFileSave();
	}
	else if (e->matches(QKeySequence::Undo))
	{
		m_pActionHandler->slotEditUndo();
	}
	else if (e->matches(QKeySequence::Redo))
	{
		m_pActionHandler->slotEditRedo();
	}
	else
	{
		switch (e->key())
		{
		case Qt::Key_Escape:    // ESC|空格 取消操作/取消选中
		case Qt::Key_Space:
		{
			GuiDocumentView* gv = m_pCurrentMdiWin->getDocumentView();
			GuiEventHandler* handle = gv->getEventHandler();
			handle->keyPressEvent(e);
			if (!e->isAccepted())
			{
				slotKillAllActions();
				//TODO. refactoring below code
				{
					m_editLine->hide();
					m_editLine->lower();
					setFocus();	
					m_cmdWin->getEditline()->setText("");
					m_cmdWin->getEdit()->setPlaceholderText(QObject::tr("command"));
				}
				e->accept();
			}
		}		
			break;

		case Qt::Key_Return:    // 大键盘回车
		case Qt::Key_Enter:     // 数字键回车
			slotEnter();
			e->accept();
			break;

		case Qt::Key_Delete:    // 键盘delete(todo:这里只能Tab+Del生效,QAction里也挂了一次)
		case Qt::Key_Backspace: // 退格(同上)
			slotDelete();
			e->accept();
			break;
		case Qt::Key_A:
		case Qt::Key_B:
		case Qt::Key_C:
		case Qt::Key_D:
		case Qt::Key_E:
		case Qt::Key_F:
		case Qt::Key_G:
		case Qt::Key_H:
		case Qt::Key_I:
		case Qt::Key_J:
		case Qt::Key_K:
		case Qt::Key_L:
		case Qt::Key_M:
		case Qt::Key_N:
		case Qt::Key_O:
		case Qt::Key_P:
		case Qt::Key_Q:
		case Qt::Key_R:
		case Qt::Key_S:
		case Qt::Key_T:
		case Qt::Key_U:
		case Qt::Key_V:
		case Qt::Key_W:
		case Qt::Key_X:
		case Qt::Key_Y:
		case Qt::Key_Z:
		case Qt::Key_0:
		case Qt::Key_1:
		case Qt::Key_2:
		case Qt::Key_3:
		case Qt::Key_4:
		case Qt::Key_5:
		case Qt::Key_6:
		case Qt::Key_7:
		case Qt::Key_8:
		case Qt::Key_9:
		case Qt::Key_Colon:
		case Qt::Key_Semicolon:
		case Qt::Key_Less:
		case Qt::Key_Greater:
		case Qt::Key_Question:
		case Qt::Key_BracketLeft:
		case Qt::Key_Backslash:
		case Qt::Key_BracketRight:
		case Qt::Key_Underscore:
		case Qt::Key_BraceLeft:
		case Qt::Key_BraceRight:
		case Qt::Key_AsciiTilde:
		case Qt::Key_Exclam:	//!
		case Qt::Key_At:		//@
		case Qt::Key_QuoteDbl:
		case Qt::Key_NumberSign:
		case Qt::Key_Dollar:	//$
		case Qt::Key_Percent:	//%
		case Qt::Key_Ampersand:	//&
		case Qt::Key_Apostrophe:
		case Qt::Key_ParenLeft:	//(
		case Qt::Key_ParenRight://)
		case Qt::Key_Asterisk:	//*
		case Qt::Key_Comma:		//,
		case Qt::Key_Period:	//.
		case Qt::Key_Slash:		///
        case Qt::Key_Plus:      // +号
        case Qt::Key_Equal:     // =号
        case Qt::Key_Minus:     // -号
			if (pointInPolygon(m_presentPos, m_pointVec))
			{
				m_editLine->raise();
				m_editLine->show();
				m_editLine->move(m_presentPos.x() + 10, m_presentPos.y() + 10);
				m_cmdWin->getEditline()->setFocus();
				m_cmdWin->getEditline()->setText(e->text());
			}
			break;
		default:
			break;
		}

		if (e->isAccepted())
		{
			return;
		}
	}

	QMainWindow::keyPressEvent(e);
}

void ApplicationWindow::slotKillAllActions()
{
	m_pCurrentMdiWin = m_pTabDrawWidget->getCurrentMdiWindow();
	GuiDocumentView* gv = m_pCurrentMdiWin->getDocumentView();
	if (gv && m_pCurrentMdiWin && m_pCurrentMdiWin->getDocument())
	{
		gv->killAllActions();

		Selection s(m_pCurrentMdiWin->getDocument(), gv);
		s.selectAll(false);
		m_pCurrentMdiWin->getDocumentView()->emitSelectedChanged();
		GUIDIALOGFACTORY->updateSelectionWidget(m_pCurrentMdiWin->getDocument()->getEntityTable()->countSelect());

		gv->redraw();
	}
}

void ApplicationWindow::slotEnter()
{
	m_pCurrentMdiWin = m_pTabDrawWidget->getCurrentMdiWindow();
	GuiDocumentView* docView = m_pCurrentMdiWin->getDocumentView();
	if (docView)
	{
		docView->enter();
	}
}

void ApplicationWindow::slotDelete()
{
	m_pActionHandler->slotModifyDeleteNoSelect();
}

void ApplicationWindow::resizeEvent(QResizeEvent* event)
{
	Q_UNUSED(event);
	if (m_pTabDrawWidget == nullptr || m_pTabDrawWidget->getCurrentMdiWindow() == nullptr)
	{
		return;
	}

	const int bottomHeight = (m_pBottomWidget && m_pBottomWidget->getWidget()) ? m_pBottomWidget->getWidget()->height() : 0;
	const int contentTop = m_pRibbon ? m_pRibbon->geometry().bottom() + 1 : m_pDrawingArea->y();
	m_pTabDrawWidget->layoutWidgets(contentTop, this->width(), this->height(), bottomHeight);
	m_pCurrentMdiWin->resize(m_pDrawingArea->size());

	// cmd
	auto m_pCommandWidget = m_cmdWin->getCommandWidget();
	m_pCommandWidget->move((this->width() - m_cmdWin->getCommandWidget()->width()) * 0.5, this->height() - 65);
	m_cmdWin->getInfoWidget()->move((this->width() - m_cmdWin->getCommandWidget()->width()) * 0.5, this->height() - 165);
	m_cmdWin->getTempWidget()->move((this->width() - m_cmdWin->getCommandWidget()->width()) * 0.5, this->height() - 65 - m_cmdWin->getTempWidget()->height());

	// 状态栏
	m_pBottomWidget->getWidget()->setFixedWidth(this->width());
	m_pBottomWidget->getWidget()->move(0, this->height() - m_pBottomWidget->getWidget()->height());
	auto bottomWidget = m_pBottomWidget->getToolWidget();
	bottomWidget->move(this->width() - bottomWidget->width(), this->height() - bottomWidget->height() - m_pBottomWidget->getWidget()->height());
	auto comboBoxWiget = m_pBottomWidget->getComboBoxWidget();
	comboBoxWiget->move(this->width() - comboBoxWiget->width(), this->height() - comboBoxWiget->height() - m_pBottomWidget->getWidget()->height());
	//auto themecomboBoxWiget = m_pBottomWidget->getThemeComBoxWidget();
	//themecomboBoxWiget->move(this->width() - themecomboBoxWiget->width(), this->height() - themecomboBoxWiget->height() - m_pBottomWidget->getWidget()->height());
	auto viewportsWidget = m_pBottomWidget->getViewportsWidget();
	viewportsWidget->move(this->width() - viewportsWidget->width() - 110, this->height() - viewportsWidget->height() - m_pBottomWidget->getWidget()->height());

	m_pointVec.clear();
	QPoint p1 = m_pDrawingArea->pos();
	m_pointVec.emplace_back(p1);
	QPoint p2(p1.x(), p1.y() + m_pDrawingArea->height());
	m_pointVec.emplace_back(p2);
	QPoint p4 = m_pCommandWidget->pos();
	QPoint p3(p4.x(), p4.y() + m_pCommandWidget->height());
	m_pointVec.emplace_back(p3);
	m_pointVec.emplace_back(p4);
	QPoint p5(p4.x() + m_pCommandWidget->width(), p4.y());
	m_pointVec.emplace_back(p5);
	QPoint p6(p5.x(), p3.y());
	m_pointVec.emplace_back(p6);
	QPoint p7(p2.x() + m_pDrawingArea->width(), p2.y());
	m_pointVec.emplace_back(p7);
	QPoint p8(p7.x(), p1.y());
	m_pointVec.emplace_back(p8);

	m_snapWidgetVec.clear();
	p1 = bottomWidget->pos();
	m_snapWidgetVec.emplace_back(p1);
	p2 = {p1.x(), p1.y() + bottomWidget->height()};
	m_snapWidgetVec.emplace_back(p2);
	p3 = { p1.x() + bottomWidget->width(), p2.y() };
	m_snapWidgetVec.emplace_back(p3);
	p4 = { p3.x(),p1.y() };
	m_snapWidgetVec.emplace_back(p4);
}

void ApplicationWindow::updateLayerTable()
{
    // 清空
    m_pLayerTableWidget->clear();
    if (nullptr == m_pCurrentMdiWin)
    {
        return;
    }

	DmDocument* document = m_pCurrentMdiWin->getDocument();
	ComboBoxData* curData = nullptr;
	DmLayer* activeLayer = document->getLayerTable()->getActive();
	// 加载当前图纸图层
	auto layerList = document->getLayerTable();
    for (auto it = layerList->begin(); it != layerList->end(); ++it)
    {
        // 创建一个图层
        auto cbxData = newLayer(*it, m_pLayerTableWidget);
        if (*it == activeLayer)
        {
            curData = cbxData;
        }
    }

    //获得选择实体个数
    DmLayer* firstSelectedLayer = nullptr;
    int selectedNumType = 0;    //0表示无选择，1表示1个图层选择，2表示多于1个图层选择
    DmEntity* singleSelectedEnt = nullptr;
    for (auto ent : *document->getEntityTable())
    {
        if (ent->isSelected())
        {
            DmLayer* layer = ent->getLayer();
            if (firstSelectedLayer == nullptr)
            {
                firstSelectedLayer = layer;
                selectedNumType = 1;
                singleSelectedEnt = ent;
            }
            else if (firstSelectedLayer != layer)
            {
                selectedNumType = 2;
                break;
            }
        }
    }

    // 更新图层UI
    auto layerItems = getLayerComboboxItems();
    if (selectedNumType == 0 || selectedNumType == 1)
    {
        ComboBoxData* toDistplayLayerData = nullptr;
        DmLayer* displayLayer = nullptr;
        if (selectedNumType == 0)
        {
            displayLayer = activeLayer;
        }
        else
        {
            displayLayer = singleSelectedEnt->getLayer();
        }
        std::find_if(layerItems.begin(), layerItems.end(), [&toDistplayLayerData, displayLayer] (CustomComboboxItem* layerItem) {
            if (layerItem->getData()->getLayerName() == displayLayer->getName())
            {
                toDistplayLayerData = layerItem->getData();
                return true;
            }
            return false;
        });
        m_pCurrentLayerItem->show();
        m_pCurrentLayerItem->setByData(toDistplayLayerData);
    }
    else if (selectedNumType == 2)
    {
        m_pCurrentLayerItem->hide();
    }

    // 收起列表
    getLayerCombox()->view()->verticalScrollBar()->setSliderPosition(0);
    getLayerCombox()->hidePopup();
    getMDIWindow()->setFocus();
}

void ApplicationWindow::updateCurrentPenWidget()
{
	if (m_pCurrentMdiWin)
	{
		m_pTabDrawWidget->updateUICurrentActivePen();
	}
}

void ApplicationWindow::slotsTabChangeEvent()
{
	//当前没有打开文档，设置UI不可用。有打开文档时，设置UI可用
	MDIWindow* oldMdiWin = m_pCurrentMdiWin;
	m_pCurrentMdiWin = m_pTabDrawWidget->getCurrentMdiWindow();
	if (nullptr == m_pCurrentMdiWin)
	{
		enableButtons(false);
	}
	else if (nullptr == oldMdiWin)
	{
		enableButtons(true);
	}
	
	updateLayerTable();
	//updateViewportTable();
    updateCurrentPenWidget();
    updateUndoRedo();
}

QAction* ApplicationWindow::getRedoAction()
{
	return m_pActRedo;
}

QAction* ApplicationWindow::getUndoAction()
{
	return m_pActUndo;
}

void ApplicationWindow::setRedoEnable(bool enable)
{
	if (m_pActRedo)
	{
		m_pActRedo->setEnabled(enable);
	}
}

void ApplicationWindow::setUndoEnable(bool enable)
{
	if (m_pActUndo)
	{
		m_pActUndo->setEnabled(enable);
	}
}

QMdiArea const* ApplicationWindow::getMdiArea() const
{
	return m_pDrawingArea;
}

QMdiArea* ApplicationWindow::getMdiArea()
{
	return m_pDrawingArea;
}

const MDIWindow* ApplicationWindow::getMDIWindow() const
{
	return m_pTabDrawWidget->getCurrentMdiWindow();
}

MDIWindow* ApplicationWindow::getMDIWindow()
{
	return m_pTabDrawWidget->getCurrentMdiWindow();
}

const GuiDocumentView* ApplicationWindow::getDocumentView() const
{
	auto mdiwindow = getMDIWindow();
	if (mdiwindow)
	{
		return mdiwindow->getDocumentView();
	}
	return nullptr;
}

GuiDocumentView* ApplicationWindow::getDocumentView()
{
	auto mdiwindow = getMDIWindow();
	if (mdiwindow)
	{
		return mdiwindow->getDocumentView();
	}
	return nullptr;
}

const DmDocument* ApplicationWindow::getDocument() const
{
	auto mdiwindow = getMDIWindow();
	if (mdiwindow)
	{
		return mdiwindow->getDocument();
	}
	return nullptr;
}

DmDocument* ApplicationWindow::getDocument()
{
	auto mdiwindow = getMDIWindow();
	if (mdiwindow)
	{
		return mdiwindow->getDocument();
	}
	return nullptr;
}

UIActionHandler* ApplicationWindow::getActionHandler() const
{
	return m_pActionHandler;
}

UITabDrawWidget* ApplicationWindow::getTabDrawWidget() const
{
	return m_pTabDrawWidget;
}

std::vector<DmDocument*> ApplicationWindow::getDocuments() const
{
	return m_pTabDrawWidget->getDocuments();
}

UICommandWidget* ApplicationWindow::getCmdWidget() const
{
	return m_cmdWin;
}

UIBottomWindow* ApplicationWindow::getBottomWidget() const
{
	return m_pBottomWidget;
}

MDIWindow* ApplicationWindow::getWindowWithDoc(const DmDocument* doc)
{
	return m_pTabDrawWidget->getCurrentMdiWindow();
}

SARibbonComboBox* ApplicationWindow::getLayerCombox() const
{
	return m_pLayerTable;
}

QListWidget* ApplicationWindow::getLayerTableWidget() const
{
	return m_pLayerTableWidget;
}

ComboBoxData* ApplicationWindow::getCurrentLayerItem() const
{
	return m_pCurrentLayerItem;
}

QAction* ApplicationWindow::getActOnOff() const
{
	return m_pActOnOff;
}

QAction* ApplicationWindow::getActLock() const
{
	return m_pActLock;
}

std::vector<CustomComboboxItem*> ApplicationWindow::getLayerComboboxItems()
{
	std::vector<CustomComboboxItem*> res;
	res.reserve(m_pLayerTableWidget->count());
	for (int i = 0; i < m_pLayerTableWidget->count(); i++)
	{
		QListWidgetItem* item = m_pLayerTableWidget->item(i);
		CustomComboboxItem* customItem = static_cast<CustomComboboxItem*>(m_pLayerTableWidget->itemWidget(item));
		res.emplace_back(customItem);
	}
	return res;
}

ApplicationWindow::~ApplicationWindow()
{
    /// @brief 必须在任何窗口、文档和全局宿主服务销毁前关闭并卸载插件。
    if (m_pluginManager)
    {
        m_pluginManager->shutdownAll();
    }
    m_pluginManager.reset();
    m_pluginUiAdapter.reset();
    m_pluginHostApi.reset();
    m_pluginRegistry.reset();
    m_pluginHostContext.reset();

	COMMANDS->deleteCommands();
	DMSETTINGS->deleteDmStettings();
	delete GuiDialogFactory::instance();
	DMSYSTEM->deleteDmSystem();
	DMFONTLIST->deleteFontList();
    DmLineTypeTable::deleteStaticLineTypes();
	DEBUG->deleteInstance();
	
	if (m_pBottomWidget)
	{
		delete m_pBottomWidget;
		m_pBottomWidget = nullptr;
	}

	if (m_pTabDrawWidget)
	{
		delete m_pTabDrawWidget;
		m_pTabDrawWidget = nullptr;
	}

	if (m_pDialogFactory)
	{
		delete m_pDialogFactory;
		m_pDialogFactory = nullptr;
	}

	if (m_pDrawingArea)
	{
		delete m_pDrawingArea;
		m_pDrawingArea = nullptr;
	}

	if (m_cmdWin)
	{
		delete m_cmdWin;
		m_cmdWin = nullptr;
	}

	if (m_pCurrentLayerItem)
	{
		delete m_pCurrentLayerItem;
		m_pCurrentLayerItem = nullptr;
	}

	Type::destruct();
}

bool ApplicationWindow::pointInPolygon(const QPoint point, const std::vector<QPoint> vec)
{
    int nCross = 0;
    auto nCount = vec.size();
    for (size_t i = 0; i < nCount; i++)
    {
        QPoint p1 = vec[i]; // 当前节点
        QPoint p2 = vec[(i + 1) % nCount]; // 下一个节点

        if (p1.y() == p2.y()) // p1p2 与 y=p0.y平行
        {
            continue;
        }

        if (point.y() < std::min(p1.y(), p2.y())) // 交点在p1p2延长线上
        {
            continue;
        }
        if (point.y() >= std::max(p1.y(), p2.y())) // 交点在p1p2延长线上
        {
            continue;
        }

        double x = static_cast<double>(point.y() - p1.y()) * static_cast<double>(p2.x() - p1.x()) / static_cast<double>(p2.y() - p1.y()) + p1.x();

        if (x > point.x())
        {
            nCross++; // 只统计单边交点
        }
    }

    // 单边交点为偶数，点在多边形之外 ---
    return (nCross % 2 == 1);
}

ApplicationWindow* ApplicationWindow::getAppWindow()
{
	return appWindow;
}

void ApplicationWindow::createCategoryFile(SARibbonCategory* page)
{
	//使用addPannel函数来创建SARibbonPannel，效果和new SARibbonPannel再addPannel一样
	SARibbonPannel* pannel1 = page->addPannel(QObject::tr("File"));

	SARibbonButtonGroupWidget* fileGroup = createRibbonButtonGroup(pannel1, 1);

	// 新建文件
	QToolButton* btnNew = new QToolButton();
	btnNew->setIcon(QIcon(":/ribbon/file/new.svg"));
	btnNew->setToolTip(QObject::tr("new"));
	btnNew->setProperty("yiCadStandaloneRibbonButton", true);
	connect(btnNew, SIGNAL(clicked()), m_pActionHandler, SLOT(slotFileNew()));
	fileGroup->addWidget(btnNew);

	// 打开文件
	QToolButton* btnOpen = new QToolButton();
	btnOpen->setIcon(QIcon(":/ribbon/file/open.svg"));
	btnOpen->setToolTip(QObject::tr("open"));
	btnOpen->setProperty("yiCadStandaloneRibbonButton", true);
	connect(btnOpen, SIGNAL(clicked()), m_pActionHandler, SLOT(slotFileOpen()));
	fileGroup->addWidget(btnOpen);

	// 保存文件
	QToolButton* btnSave = new QToolButton();
	btnSave->setIcon(QIcon(":/ribbon/file/save.svg"));
	btnSave->setToolTip(QObject::tr("save"));
	btnSave->setProperty("yiCadStandaloneRibbonButton", true);
	connect(btnSave, SIGNAL(clicked()), m_pActionHandler, SLOT(slotFileSave()));
	fileGroup->addWidget(btnSave);

	// 另存为
	QToolButton* btnSaveAs = new QToolButton();
	btnSaveAs->setIcon(QIcon(":/ribbon/file/save_as.svg"));
	btnSaveAs->setToolTip(QObject::tr("save as"));
	btnSaveAs->setProperty("yiCadStandaloneRibbonButton", true);
	connect(btnSaveAs, SIGNAL(clicked()), m_pActionHandler, SLOT(slotFileSaveAs()));
	fileGroup->addWidget(btnSaveAs);

	//// 保存全部
	//QToolButton* btnSaveAll = new QToolButton();
	//btnSaveAll->setIcon(QIcon(":/ribbon/file/save_all.svg"));
	//btnSaveAll->setToolTip(QObject::tr("save all"));
	//connect(btnSaveAll, &QToolButton::clicked, this, [this](bool b) {
	//	Q_UNUSED(b);
	//	m_pTabDrawWidget->slotFileSaveAll();
	//});
	//fileGroup->addWidget(btnSaveAll);

	pannel1->addLargeWidget(fileGroup);


	SARibbonPannel* pannel2 = page->addPannel(QObject::tr("Export"));

	//SARibbonButtonGroupWidget* importGroup = createRibbonButtonGroup(pannel2, 1);

	//// 导入图片
	//QToolButton* btnImportImage = new QToolButton();
	//btnImportImage->setIcon(QIcon(":/ribbon/file/import_image.svg"));
	//btnImportImage->setToolTip(QObject::tr("Import Image"));
	//connect(btnImportImage, &QToolButton::clicked, this, [this](bool b) {
	//	Q_UNUSED(b);
	//	m_pActionHandler->slotDrawImage();
	//});
	//importGroup->addWidget(btnImportImage);

	//// 导入图块
	//QToolButton* btnImportBlock = new QToolButton();
	//btnImportBlock->setIcon(QIcon(":/ribbon/file/import_block.svg"));
	//btnImportBlock->setToolTip(QObject::tr("Import Block"));
	//connect(btnImportBlock, &QToolButton::clicked, this, [this](bool b) {
	//	Q_UNUSED(b);
	//	m_pTabDrawWidget->slotImportBlock();
	//});
	//importGroup->addWidget(btnImportBlock);

	//pannel2->addLargeWidget(importGroup);

	SARibbonButtonGroupWidget* exportGroup = createRibbonButtonGroup(pannel2, 1);

	// 导出图片
	QToolButton* btnExportImage = new QToolButton();
	btnExportImage->setIcon(QIcon(":/ribbon/file/export_image.svg"));
	btnExportImage->setToolTip(QObject::tr("Export Image"));
	btnExportImage->setProperty("yiCadStandaloneRibbonButton", true);
	connect(btnExportImage, SIGNAL(clicked()), m_pActionHandler, SLOT(slotFileExportImage()));
	exportGroup->addWidget(btnExportImage);

	//// 导出PDF
	//QToolButton* btnExportPDF = new QToolButton();
	//btnExportPDF->setIcon(QIcon(":/ribbon/file/export_pdf.svg"));
	//btnExportPDF->setToolTip(QObject::tr("Export PDF"));
	//connect(btnExportPDF, &QToolButton::clicked, this, [this](bool b) {
	//	Q_UNUSED(b);
	//	m_pTabDrawWidget->slotFilePrintPDF();
	//});
	//exportGroup->addWidget(btnExportPDF);

	pannel2->addLargeWidget(exportGroup);



	//SARibbonPannel* pannel3 = page->addPannel(QObject::tr("Print"));

	//SARibbonButtonGroupWidget* printGroup = createRibbonButtonGroup(pannel3, 1);

	//// 打印
	//QToolButton* btnPrint = new QToolButton();
	//btnPrint->setIcon(QIcon(":/ribbon/file/print.svg"));
	//btnPrint->setToolTip(QObject::tr("Print"));
	//connect(btnPrint, &QToolButton::clicked, this, [this](bool b) {
	//	Q_UNUSED(b);
	//	m_pTabDrawWidget->slotFilePrint();
	//});
	//printGroup->addWidget(btnPrint);

	//// 打印预览
	//QToolButton* btnPrintPreview = new QToolButton();
	//btnPrintPreview->setIcon(QIcon(":/ribbon/file/print_preview.svg"));
	//btnPrintPreview->setToolTip(QObject::tr("Print Preview"));
	//connect(btnPrintPreview, &QToolButton::clicked, this, [this](bool b) {
	//	Q_UNUSED(b);
	//	m_pTabDrawWidget->slotFilePrintPreview();
	//});
	//printGroup->addWidget(btnPrintPreview);

	//pannel3->addLargeWidget(printGroup);


	//SARibbonPannel* pannel4 = page->addPannel(QObject::tr("Close"));

	//SARibbonButtonGroupWidget* closeGroup = createRibbonButtonGroup(pannel4, 1);

	//// 关闭所有图纸
	//QToolButton* btnCloseAll = new QToolButton();
	//btnCloseAll->setIcon(QIcon(":/ribbon/file/close_all.svg"));
	//btnCloseAll->setToolTip(QObject::tr("Close All"));
	//connect(btnCloseAll, &QToolButton::clicked, this, [this](bool b) {
	//	Q_UNUSED(b);
	//	m_pTabDrawWidget->slotFileCloseAll();
	//});
	//closeGroup->addWidget(btnCloseAll);

	//// 退出
	//QToolButton* btnQuit = new QToolButton();
	//btnQuit->setIcon(QIcon(":/ribbon/file/quit.svg"));
	//btnQuit->setToolTip(QObject::tr("Quit"));
	//connect(btnQuit, &QToolButton::clicked, this, [this](bool b) {
	//	Q_UNUSED(b);
	//	m_pTabDrawWidget->slotFileCloseAll();
	//	if (m_pTabDrawWidget->getTabDrawList()->size() == 0)
	//	{
	//		close();
	//	}
	//});
	//closeGroup->addWidget(btnQuit);

	//pannel4->addLargeWidget(closeGroup);
}

void ApplicationWindow::createCategoryOptions(SARibbonCategory* page)
{
	SARibbonPannel* pannel1 = new SARibbonPannel(QObject::tr("Options"));
	pannel1->setObjectName("CategoryOptions-pannel1");
	page->addPannel(pannel1);

	SARibbonButtonGroupWidget* settingGroup = createRibbonButtonGroup(pannel1, 1);

	//系统设置
	QToolButton* btnApplicationSettings = new QToolButton();
	btnApplicationSettings->setIcon(QIcon(":/ribbon/options/settings.svg"));
	btnApplicationSettings->setToolTip(QObject::tr("System Setting"));
	btnApplicationSettings->setProperty("yiCadStandaloneRibbonButton", true);
	connect(btnApplicationSettings, SIGNAL(clicked()), m_pActionHandler, SLOT(slotOptionsGeneral()));
	settingGroup->addWidget(btnApplicationSettings);

	//图纸设置
	QToolButton* btnDrawSettings = new QToolButton();
	btnDrawSettings->setIcon(QIcon(":/ribbon/options/draw_settings.svg"));
	btnDrawSettings->setToolTip(QObject::tr("Draw Setting"));
	btnDrawSettings->setProperty("yiCadStandaloneRibbonButton", true);
	connect(btnDrawSettings, &QToolButton::clicked, this, [this](bool b) {
		Q_UNUSED(b);
		m_pActionHandler->slotOptionsDrawing();
	});
	settingGroup->addWidget(btnDrawSettings);

	pannel1->addLargeWidget(settingGroup);

	//// 关于菜单
	//SARibbonPannel* pannel2 = new SARibbonPannel(QObject::tr("About"));
	//pannel2->setObjectName("CategoryOptions-pannel2");
	//page->addPannel(pannel2);

	//SARibbonButtonGroupWidget* aboutGroup = createRibbonButtonGroup(pannel2, 1);

	//QToolButton* about = new QToolButton();
	//about->setIcon(QIcon(":/ribbon/options/about.svg"));
	//about->setToolTip(QObject::tr("About"));
	//connect(about, &QToolButton::clicked, this, [this](bool b) {
	//	QMessageBox::information(this, QString::fromLocal8Bit("关于"), QString::fromLocal8Bit("<pre>易CAD(禁止转售)</pre><pre>版本 0.5(x64)</pre><pre>版权由易设计CAD所有</pre>"));
	//});
	//aboutGroup->addWidget(about);

	//pannel2->addLargeWidget(aboutGroup);
}

/// @brief 构建绘图标签页
void ApplicationWindow::createCategoryDraw2d(SARibbonCategory* page)
{
	// 画线
	SARibbonPannel* line = page->addPannel(QObject::tr("Line"));
	SARibbonButtonGroupWidget* lineGroup = createRibbonButtonGroup(line);
	auto point2p_l = createAction(tr("2 Points"), ":/ribbon/draw2d/line_2p.svg");
	connect(point2p_l, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawLine()));
	lineGroup->addAction(point2p_l);																				// 两点画线

	auto rectangle = createAction(QObject::tr("Rectangle"), ":/ribbon/draw2d/line_square.svg");
	connect(rectangle, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawLineRectangle()));
	lineGroup->addAction(rectangle);																				// 矩形

	auto bisector = createAction(QObject::tr("Bisector"), ":/ribbon/draw2d/line_bi_angle.svg");
	connect(bisector, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawLineBisector()));
	lineGroup->addAction(bisector);																					// 角平分线

	auto tangent_p_c = createAction(QObject::tr("Tangent (P,C)"), ":/ribbon/draw2d/line_tangent_pt_circle.svg");
	connect(tangent_p_c, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawLineTangent1()));
	lineGroup->addAction(tangent_p_c);																				// 切线(点、圆)

	auto tangent_c_c = createAction(QObject::tr("Tangent (C,C)"), ":/ribbon/draw2d/line_tangent_c_c.svg");
	connect(tangent_c_c, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawLineTangent2()));
	lineGroup->addAction(tangent_c_c);																				// 切线(圆、圆)

	auto tangent_orth = createAction(QObject::tr("Tangent Orthogonal"), ":/ribbon/draw2d/line_tan_orthognal.svg");
	connect(tangent_orth, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawLineOrthTan()));
	lineGroup->addAction(tangent_orth);																				// 正交切线

	auto polygon_cen_cor = createAction(QObject::tr("Polygon (Cen,Cor)"), ":/ribbon/draw2d/line_polygon_cen_cor.svg");
	connect(polygon_cen_cor, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawLinePolygon()));
	lineGroup->addAction(polygon_cen_cor);																			// 多边形(中心、角点)

	auto polygon_cen_tan = createAction(QObject::tr("Polygon (Cen,Tan)"), ":/ribbon/draw2d/line_polygon_cen_tan.svg");
	connect(polygon_cen_tan, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawLinePolygon3()));
	lineGroup->addAction(polygon_cen_tan);																			// 多边形(中心、切点)

	//auto point = createAction(QObject::tr("Points"), ":/ribbon/draw2d/point.svg");
	//connect(point, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawPoint()));
	//lineGroup->addAction(point);																					// 点

	auto ray = createAction(QObject::tr("Ray"), ":/ribbon/draw2d/line_ray.svg");									// 射线
	connect(ray, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawRay()));
	lineGroup->addAction(ray);

	auto xline = createAction(QObject::tr("Xline"), ":/ribbon/draw2d/line_xline.svg");								// 构造线
	connect(xline, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawXline()));
	lineGroup->addAction(xline);

	line->addLargeWidget(lineGroup);

	// 曲线
	SARibbonPannel* curve = page->addPannel(QObject::tr("Curve"));
	SARibbonButtonGroupWidget* curveGroup = createRibbonButtonGroup(curve);

	auto curve_cpa = createAction(QObject::tr("Center, Point, Angles"), ":/ribbon/draw2d/curve_arc_center_angle.svg");
	connect(curve_cpa, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawArc()));
	curveGroup->addAction(curve_cpa);																					// 中心起点角度

	auto curve_3p = createAction(QObject::tr("3 Points"), ":/ribbon/draw2d/curve_arc_3p.svg");
	connect(curve_3p, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawArc3P()));
	curveGroup->addAction(curve_3p);																					// 三点画弧

	auto curve_at = createAction(QObject::tr("Arc Tangential"), ":/ribbon/draw2d/curve_arc_tang.svg");
	connect(curve_at, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawArcTangential()));
	curveGroup->addAction(curve_at);																					// 相切弧

	auto spline = createAction(QObject::tr("Spline"), ":/ribbon/draw2d/curve_spline.svg");
	connect(spline, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawSpline()));
	curveGroup->addAction(spline);																						// 控制点样条

	auto spline_through_points = createAction(QObject::tr("Spline through points"), ":/ribbon/draw2d/curve_spline_ft_pt.svg");
	connect(spline_through_points, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawSplinePoints()));
	curveGroup->addAction(spline_through_points);																		// 拟合点样条

	//auto ellipse_arc = createAction(QObject::tr("Ellipse Arc(Axis)"), ":/ribbon/draw2d/curve_ellipse_arc.svg");
	//connect(ellipse_arc, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawEllipseArcAxis()));
	//curveGroup->addAction(ellipse_arc);																					// 椭圆弧(轴)

	auto freehand = createAction(QObject::tr("Freehand Line"), ":/ribbon/draw2d/curve_freehand_line.svg");
	connect(freehand, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawLineFree()));
	curveGroup->addAction(freehand);																					// 徒手画线

	curve->addLargeWidget(curveGroup);

	// 多段线
	SARibbonPannel* polyline = page->addPannel(QObject::tr("Polyline"));
	SARibbonButtonGroupWidget* polylineGroup = createRibbonButtonGroup(polyline);

	auto polylinedraw = createAction(QObject::tr("Polyline"), ":/ribbon/draw2d/polyline.svg");
	connect(polylinedraw, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawPolyline()));
	polylineGroup->addAction(polylinedraw);																					// 多段线

	auto polyline_add_node = createAction(QObject::tr("Add node"), ":/ribbon/draw2d/polyline_add_node.svg");
	connect(polyline_add_node, SIGNAL(triggered()), m_pActionHandler, SLOT(slotPolylineAdd()));
	polylineGroup->addAction(polyline_add_node);																			// 添加节点

	auto polyline_app_node = createAction(QObject::tr("Append node"), ":/ribbon/draw2d/polyline_append_node.svg");
	connect(polyline_app_node, SIGNAL(triggered()), m_pActionHandler, SLOT(slotPolylineAppend()));
	polylineGroup->addAction(polyline_app_node);																			// 追加节点

	auto polyline_delete_node = createAction(QObject::tr("Delete node"), ":/ribbon/draw2d/polyline_delete_node.svg");
	connect(polyline_delete_node, SIGNAL(triggered()), m_pActionHandler, SLOT(slotPolylineDel()));
	polylineGroup->addAction(polyline_delete_node);																			// 删除节点

	auto cloudline_rectangle_cpfe = createAction(QObject::tr("Create cloud line by rectangle"), ":/ribbon/draw2d/cloudline_rectangle.svg");
	connect(cloudline_rectangle_cpfe, SIGNAL(triggered()), m_pActionHandler, SLOT(slotCloudLineRectangle()));
	polylineGroup->addAction(cloudline_rectangle_cpfe);																		// 矩形创建云线

	auto cloudline_polygon_cpfe = createAction(QObject::tr("Create cloud line by polygon"), ":/ribbon/draw2d/cloudline_polygon.svg");
	connect(cloudline_polygon_cpfe, SIGNAL(triggered()), m_pActionHandler, SLOT(slotCloudLinePolygon()));
	polylineGroup->addAction(cloudline_polygon_cpfe);																		// 多边形创建云线

	auto cloudline_free_cpfe = createAction(QObject::tr("Create cloud line by free"), ":/ribbon/draw2d/cloudline_free.svg");
	connect(cloudline_free_cpfe, SIGNAL(triggered()), m_pActionHandler, SLOT(slotCloudLineFree()));
	polylineGroup->addAction(cloudline_free_cpfe);																			// 自由创建云线

	polyline->addLargeWidget(polylineGroup);

	// 画圆
	SARibbonPannel* circle = page->addPannel(QObject::tr("Circle"));
	SARibbonButtonGroupWidget* circleGroup = createRibbonButtonGroup(circle);

	auto circle_c_p = createAction(QObject::tr("Center, Point"), ":/ribbon/draw2d/circle_center_pt.svg");
	connect(circle_c_p, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawCircle()));
	circleGroup->addAction(circle_c_p);																					// 圆心到点

	auto point2p_c = createAction(QObject::tr("2 Points"), ":/ribbon/draw2d/circle_2p.svg");
	connect(point2p_c, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawCircle2P()));
	circleGroup->addAction(point2p_c);																					// 两点画圆

	auto point3p = createAction(QObject::tr("3 Points"), ":/ribbon/draw2d/circle_3p.svg");
	connect(point3p, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawCircle3P()));
	circleGroup->addAction(point3p);																					// 三点画圆

	auto circle_t2cr = createAction(QObject::tr("Tangential 2 Circles, Radius"), ":/ribbon/draw2d/circle_tan_radius.svg");
	connect(circle_t2cr, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawCircleTan2()));
	circleGroup->addAction(circle_t2cr);																				// 相切两圆半径画圆

	auto circle_t3c = createAction(QObject::tr("Tangential 3 Circles"), ":/ribbon/draw2d/circle_tan_3c.svg");
	connect(circle_t3c, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawCircleTan3()));
	circleGroup->addAction(circle_t3c);																					// 相切三圆

	circle->addLargeWidget(circleGroup);

	// 椭圆
	SARibbonPannel* ellipse = page->addPannel(QObject::tr("Ellipse"));
	SARibbonButtonGroupWidget* ellipseGroup = createRibbonButtonGroup(ellipse);

	auto ellipse_a = createAction(QObject::tr("Ellipse(Axis)"), ":/ribbon/draw2d/ellipe_2seg.svg");
	connect(ellipse_a, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawEllipseAxis()));
	ellipseGroup->addAction(ellipse_a);																						// 椭圆(长短轴)

	auto ellipse_i = createAction(QObject::tr("Ellipse Inscribed"), ":/ribbon/draw2d/ellipse_incrib.svg");
	connect(ellipse_i, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawEllipseInscribe()));
	ellipseGroup->addAction(ellipse_i);																						// 内切椭圆

	ellipse->addLargeWidget(ellipseGroup);

	// 标注
	SARibbonPannel* dimension = page->addPannel(QObject::tr("Dimension"));
	SARibbonButtonGroupWidget* dimensionGroup = createRibbonButtonGroup(dimension);

	auto dim_aligned = createAction(QObject::tr("Aligned"), ":/ribbon/draw2d/dim_align.svg");
	connect(dim_aligned, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDimAligned()));
	dimensionGroup->addAction(dim_aligned);																					// 对齐标注

	auto dim_linear = createAction(QObject::tr("Linear"), ":/ribbon/draw2d/dim_linear.svg");
	connect(dim_linear, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDimLinear()));
	dimensionGroup->addAction(dim_linear);																					// 线性标注

	auto dim_radial = createAction(QObject::tr("Radial"), ":/ribbon/draw2d/dim_radius.svg");
	connect(dim_radial, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDimRadial()));
	dimensionGroup->addAction(dim_radial);																					// 半径标注

	auto dim_diametric = createAction(QObject::tr("Diametric"), ":/ribbon/draw2d/dim_diam.svg");
	connect(dim_diametric, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDimDiametric()));
	dimensionGroup->addAction(dim_diametric);																				// 直径标注

	auto dim_angluar = createAction(QObject::tr("Angluar"), ":/ribbon/draw2d/dim_angle.svg");
	connect(dim_angluar, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDimAngular()));
	dimensionGroup->addAction(dim_angluar);																					// 角度标注

	auto dim_leader = createAction(QObject::tr("Leader"), ":/ribbon/draw2d/dim_leader.svg");
	connect(dim_leader, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDimLeader()));
	dimensionGroup->addAction(dim_leader);																					// 引线标注

	auto dim_baseline = createAction(QObject::tr("Baseline"), ":/ribbon/draw2d/dim_baseline.svg");
	connect(dim_baseline, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDimBaseline()));
	dimensionGroup->addAction(dim_baseline);																					// 基线标注

	QAction* dim_style = createAction(QObject::tr("Dimension style"), ":/ribbon/draw2d/dim_style.svg");
	connect(dim_style, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDimStyle()));
	dimensionGroup->addAction(dim_style);														//标注样式

	dimension->addLargeWidget(dimensionGroup);

	// 文字
	SARibbonPannel* text = page->addPannel(QObject::tr("Text"));
	SARibbonButtonGroupWidget* textGroup = createRibbonButtonGroup(text);

	auto other_singl_texe = createAction(QObject::tr("Single line text"), ":/ribbon/draw2d/text.svg");
	connect(other_singl_texe, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawText()));
	textGroup->addAction(other_singl_texe);																						// 单行文字

	auto other_multiline_text = createAction(QObject::tr("Multiline text"), ":/ribbon/draw2d/mtext.svg");
	connect(other_multiline_text, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawMText()));
	textGroup->addAction(other_multiline_text);																					// 多行文字

	QAction* other_text_style = createAction(QObject::tr("Text style"), ":/ribbon/draw2d/text_style.svg");
	connect(other_text_style, SIGNAL(triggered()), m_pActionHandler, SLOT(slotTextStyle()));
	textGroup->addAction(other_text_style);																						// 文字样式

	text->addLargeWidget(textGroup);

	// 其他
	SARibbonPannel* other = page->addPannel(QObject::tr("Other"));
	SARibbonButtonGroupWidget* otherGroup = createRibbonButtonGroup(other);

	auto other_hatch = createAction(QObject::tr("Hatch"), ":/ribbon/draw2d/hatch.svg");
	connect(other_hatch, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawHatch()));
	otherGroup->addAction(other_hatch);																							// 填充

	auto other_ins_image = createAction(QObject::tr("Insert Image"), ":/ribbon/draw2d/insert_image.svg");
	connect(other_ins_image, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDrawImage()));
	otherGroup->addAction(other_ins_image);																						// 插入图片

	other->addLargeWidget(otherGroup);

	//! --- 修改 ---
	SARibbonPannel* modify = page->addPannel(QObject::tr("Modify"));
	SARibbonButtonGroupWidget* modifyGroup = createRibbonButtonGroup(modify);
    auto modify_copy = createAction(QObject::tr("Copy"), ":/ribbon/draw2d/modify_copy.svg");
    connect(modify_copy, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyCopy()));
    modifyGroup->addAction(modify_copy);																					// 复制

	auto modify_move = createAction(QObject::tr("Move"), ":/ribbon/draw2d/modify_move.svg");
	connect(modify_move, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyMove()));
	modifyGroup->addAction(modify_move);																					// 移动

	auto modify_rotate = createAction(QObject::tr("Rotate"), ":/ribbon/draw2d/modify_rotate.svg");
	connect(modify_rotate, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyRotate()));
	modifyGroup->addAction(modify_rotate);																				// 旋转

	auto modify_select = createAction(QObject::tr("Scale"), ":/ribbon/draw2d/modify_zoom.svg");
	connect(modify_select, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyScale()));
	modifyGroup->addAction(modify_select);																				// 放缩

	auto modify_mirror = createAction(QObject::tr("Mirror"), ":/ribbon/draw2d/modify_mirror.svg");
	connect(modify_mirror, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyMirror()));
	modifyGroup->addAction(modify_mirror);																				// 镜像

	auto modify_t = createAction(QObject::tr("Trim"), ":/ribbon/draw2d/modify_trim.svg");
	connect(modify_t, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyTrim()));
	modifyGroup->addAction(modify_t);																					// 修剪

	auto modify_len = createAction(QObject::tr("Lengthen"), ":/ribbon/draw2d/modify_lengthen.svg");						// 延伸
	connect(modify_len, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyExtend()));
	modifyGroup->addAction(modify_len);

	auto modify_offset = createAction(QObject::tr("Offset"), ":/ribbon/draw2d/modify_offset.svg");
	connect(modify_offset, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifySingleOffset()));
	modifyGroup->addAction(modify_offset);																				// 偏移

	auto modify_bevel = createAction(QObject::tr("Bevel"), ":/ribbon/draw2d/modify_bevel.svg");
	connect(modify_bevel, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyBevel()));
	modifyGroup->addAction(modify_bevel);																				// 倒角

	auto modify_fillet = createAction(QObject::tr("Fillet"), ":/ribbon/draw2d/modify_fillet.svg");
	connect(modify_fillet, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyRound()));
	modifyGroup->addAction(modify_fillet);																				// 圆角

	auto modify_divide = createAction(QObject::tr("Divide"), ":/ribbon/draw2d/modify_divide.svg");
	connect(modify_divide, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyCut()));
	modifyGroup->addAction(modify_divide);																				// 打断

	auto modify_divide_2p = createAction(QObject::tr("Divide_2P"), ":/ribbon/draw2d/modify_twopoints_break.svg");
	connect(modify_divide_2p, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyCut_2P()));                        //两点打断
	modifyGroup->addAction(modify_divide_2p);

	auto modify_properties = createAction(QObject::tr("Properties"), ":/ribbon/draw2d/modify_attributes.svg");
	connect(modify_properties, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyEntity()));
	modifyGroup->addAction(modify_properties);																			// 特性

	auto modify_ex = createAction(QObject::tr("Explode"), ":/ribbon/draw2d/modify_explode.svg");
	connect(modify_ex, SIGNAL(triggered()), m_pActionHandler, SLOT(slotModifyExplode()));
	modifyGroup->addAction(modify_ex);																					// 分解

	modify->addLargeWidget(modifyGroup);

	// 测量
	SARibbonPannel* info = page->addPannel(QObject::tr("Measure"));
	SARibbonButtonGroupWidget* infoGroup = createRibbonButtonGroup(info);

	auto info_dis_pp = createAction(QObject::tr("Distance Point to Point"), ":/ribbon/draw2d/info_dist_pt_pt.svg");
	connect(info_dis_pp, SIGNAL(triggered()), m_pActionHandler, SLOT(slotInfoDist()));
	infoGroup->addAction(info_dis_pp);																							// 点到点距离

	auto info_angle_lines = createAction(QObject::tr("Angle between two lines"), ":/ribbon/draw2d/info_angle_2lines.svg");
	connect(info_angle_lines, SIGNAL(triggered()), m_pActionHandler, SLOT(slotInfoAngle()));
	infoGroup->addAction(info_angle_lines);																						// 两线夹角

	auto info_total = createAction(QObject::tr("Total length of selected entities"), ":/ribbon/draw2d/info_length_entity.svg");
	connect(info_total, SIGNAL(triggered()), m_pActionHandler, SLOT(slotInfoTotalLength()));
	infoGroup->addAction(info_total);																							// 选中实体总长

	auto info_polygon_area = createAction(QObject::tr("Polygonal Area"), ":/ribbon/draw2d/info_area_polygon.svg");
	connect(info_polygon_area, SIGNAL(triggered()), m_pActionHandler, SLOT(slotInfoArea()));
	infoGroup->addAction(info_polygon_area);																					// 多边形面积

	info->addLargeWidget(infoGroup);


	// 图层
	SARibbonPannel* layer = page->addPannel(QObject::tr("Layer"));
	// 创建图层列表
	createLayerTable(layer);
	
	// 图块
	SARibbonPannel* block = page->addPannel(QObject::tr("Block"));
	SARibbonButtonGroupWidget* blockGroup = createRibbonButtonGroup(block);

	// 创建图块
	auto createBlock = createAction(QObject::tr("Create Block"), ":/ribbon/block/block_create.svg");
	blockGroup->addAction(createBlock);
	connect(createBlock, SIGNAL(triggered()), m_pActionHandler, SLOT(slotBlocksCreate()));
	
	// 插入图块
	auto insertBlock = createAction(QObject::tr("Insert the active block"), ":/ribbon/block/block_insert.svg");
	blockGroup->addAction(insertBlock);
	connect(insertBlock, SIGNAL(triggered()), m_pActionHandler, SLOT(slotBlocksInsertPrepare()));

	// 图块另存为
	auto exportBlock = createAction(QObject::tr("save the block to a file"), ":/ribbon/block/block_save.svg");
	blockGroup->addAction(exportBlock);
	connect(exportBlock, SIGNAL(triggered()), m_pActionHandler, SLOT(slotBlocksSaveAs()));

	//属性定义
	auto defineAttributes = createAction(QObject::tr("Define attributes"), ":/ribbon/block/define_attribute.svg");
	connect(defineAttributes, SIGNAL(triggered()), m_pActionHandler, SLOT(slotDefineAttributes()));
	blockGroup->addAction(defineAttributes);																					// 属性定义

	// 删除图块
	auto deleteBlock = createAction(QObject::tr("Delete Block"), ":/ribbon/block/block_delete.svg");
	blockGroup->addAction(deleteBlock);
	connect(deleteBlock, SIGNAL(triggered()), m_pActionHandler, SLOT(slotBlocksDelete()));

	// 编辑图块
	auto editBlock = createAction(QObject::tr("Edit Block"), ":/ribbon/block/block_edit.svg");
	blockGroup->addAction(editBlock);
	connect(editBlock, SIGNAL(triggered()), m_pActionHandler, SLOT(slotBlocksEdit()));

	// 导入图块
	auto importBlock = createAction(QObject::tr("Import Block"), ":/ribbon/file/import_block.svg");
	blockGroup->addAction(importBlock);
	connect(importBlock, SIGNAL(triggered()), m_pActionHandler, SLOT(slotBlocksImport()));

	block->addLargeWidget(blockGroup);
}

void ApplicationWindow::createCategorySolver(SARibbonCategory* page)
{
	// 约束
	SARibbonPannel* solver = page->addPannel(QObject::tr("Geometry"));
	SARibbonButtonGroupWidget* solverGroup = createRibbonButtonGroup(solver);

	auto solver_vertical = createAction(QObject::tr("Solver Vertical(p)"), ":/ribbon/solver/solver_vertical_p.svg");
	solverGroup->addAction(solver_vertical);															// 垂直

	auto solver_smooth = createAction(QObject::tr("Solver Smooth"), ":/ribbon/solver/solver_smooth.svg");
	solverGroup->addAction(solver_smooth);																// 平滑

	auto solver_parallel = createAction(QObject::tr("Solver Parallel"), ":/ribbon/solver/solver_parallel.svg");
	solverGroup->addAction(solver_parallel);															// 平行

	auto solver_vertical_v = createAction(QObject::tr("Solver Vertical(v)"), ":/ribbon/solver/solver_vertical_v.svg");
	solverGroup->addAction(solver_vertical_v);															// 竖直

	auto solver_horizontal = createAction(QObject::tr("Solver Horizontal"), ":/ribbon/solver/solver_horizontal.svg");
	solverGroup->addAction(solver_horizontal);															// 水平

	auto solver_concentric = createAction(QObject::tr("Solver Concentric"), ":/ribbon/solver/solver_concentric.svg");
	solverGroup->addAction(solver_concentric);															// 同心

	auto solver_equal = createAction(QObject::tr("Solver Equal"), ":/ribbon/solver/solver_equal.svg");
	solverGroup->addAction(solver_equal);																// 相等

	auto solver_tangent = createAction(QObject::tr("Solver Tangent"), ":/ribbon/solver/solver_tangent.svg");
	solverGroup->addAction(solver_tangent);																// 相切

	auto solver_right_angle = createAction(QObject::tr("Solver Right Angle"), ":/ribbon/solver/solver_right_angle.svg");
	solverGroup->addAction(solver_right_angle);															// 直角

	auto solver_coincide = createAction(QObject::tr("Solver Coincide"), ":/ribbon/solver/solver_coincide.svg");
	solverGroup->addAction(solver_coincide);															// 重合

	solver->addLargeWidget(solverGroup);
}

/// @brief 顶部导航条
void ApplicationWindow::createQuickAccessBar(SARibbonQuickAccessBar* quickAccessBar)
{
	auto actNew = createAction(QObject::tr("new"), ":/ribbon/file/new.svg", "new-quickbar");
	connect(actNew, SIGNAL(triggered()), m_pActionHandler, SLOT(slotFileNew()));
	quickAccessBar->addAction(actNew);																		// 新建

	auto actOpen = createAction(QObject::tr("open"), ":/ribbon/file/open.svg", "open-quickbar");
	connect(actOpen, &QAction::triggered, this, [this](bool b) {
		Q_UNUSED(b);
		DMSYSTEM->setCurrentFormatType("ycd");
		m_pTabDrawWidget->slotFileOpen();
		m_pActionHandler->slotSetSnaps(m_pBottomWidget->getSnapToolBar()->getSnaps());
		});
	quickAccessBar->addAction(actOpen);																		// 打开

	auto actSave = createAction(QObject::tr("save"), ":/ribbon/file/save.svg", "save-quickbar");
	connect(actSave, SIGNAL(triggered()), m_pActionHandler, SLOT(slotFileSave()));
	quickAccessBar->addAction(actSave);																		// 保存

	auto actSaveas = createAction(QObject::tr("save as"), ":/ribbon/file/save_as.svg", "saveas-quickbar");
	connect(actSaveas, SIGNAL(triggered()), m_pActionHandler, SLOT(slotFileSaveAs()));
	quickAccessBar->addAction(actSaveas);																	// 另存为
	quickAccessBar->addSeparator();																			// 分割条

	// undo/redo
	// 撤销
	m_pActUndo = createAction(QObject::tr("Undo"), ":/ribbon/undo.svg");
	connect(m_pActUndo, SIGNAL(triggered()), m_pActionHandler, SLOT(slotEditUndo()));
    m_pActUndo->setEnabled(false);
	quickAccessBar->addAction(m_pActUndo);																	// 回退

	// 重做
	m_pActRedo = createAction(QObject::tr("Redo"), ":/ribbon/redo.svg");
	connect(m_pActRedo, SIGNAL(triggered()), m_pActionHandler, SLOT(slotEditRedo()));
    m_pActRedo->setEnabled(false);
	quickAccessBar->addAction(m_pActRedo);																	// 重做
	quickAccessBar->addSeparator();                                                                         // 分割条
	//QMenu* m = new QMenu("Recent Files", this);                                                        // 历史文件 todo: 暂时屏蔽
	//m->setIcon(QIcon(":/ribbon/recent_files.svg"));
	//for (int i = 0; i < 10; ++i)
	//{
	//	m->addAction(createAction(QString("file%1").arg(i + 1), ":/ribbon/presentationFile.svg"));
	//}
	//quickAccessBar->addMenu(m);
}

QAction* ApplicationWindow::createAction(const QString& text, const QString& iconurl, const QString& objName)
{
	QAction* act = new QAction(this);
	act->setText(text);
	act->setIcon(QIcon(iconurl));
	act->setObjectName(objName);
	return act;
}

QAction* ApplicationWindow::createAction(const QString& text, const QString& iconurl)
{
	QAction* act = new QAction(this);
	act->setText(text);
	act->setIcon(QIcon(iconurl));
	act->setObjectName(text);
	return act;
}

void ApplicationWindow::createLayerTable(SARibbonPannel* layerPannel)
{
 	SARibbonButtonGroupWidget* allGroup = createRibbonButtonGroup(layerPannel);

	// 图层列表
	m_pLayerTable = new SARibbonComboBox(allGroup);
	m_pLayerTable->setMinimumSize(200, 23);
	m_pLayerTable->setEditable(false);	// 禁止输入
	m_pLayerTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
	
	m_pLayerTableWidget = new QListWidget(m_pLayerTable);
	m_pLayerTable->setModel(m_pLayerTableWidget->model());
	m_pLayerTable->setView(m_pLayerTableWidget);

	// 设置下拉列表当前显示数据
	auto currentLayer = m_pCurrentMdiWin->getDocument()->getLayerTable()->getActive();
	m_pCurrentLayerItem = initLayerComboboxItem(currentLayer, m_pLayerTable, true);

	// 加载当前图纸图层
	auto layerList = m_pCurrentMdiWin->getDocument()->getLayerTable();
    for (auto it = layerList->begin(); it != layerList->end(); ++it)
    {
        // 创建一个图层
        newLayer(*it, m_pLayerTableWidget);
    }
	allGroup->addWidget(m_pLayerTable);

	// 打开所有图层
	m_pActOnOff = createAction(QObject::tr("on all"), ":/ribbon/layer/layer_all_visible.svg");
	connect(m_pActOnOff, SIGNAL(triggered()), m_pActionHandler, SLOT(slotLayersDefreezeAll()));
	
	// 解锁所有图层
	m_pActLock = createAction(QObject::tr("unlock all"), ":/ribbon/layer/layer_all_unlock.svg");
	connect(m_pActLock, SIGNAL(triggered()), m_pActionHandler, SLOT(slotLayersUnlockAll()));
	
	// 新增图层
	QAction* actNewLayer = createAction(QObject::tr("new layer"), ":/ribbon/layer/add_layer.svg");
	connect(actNewLayer, SIGNAL(triggered()), m_pActionHandler, SLOT(slotLayersAdd()));

	//复制实体到图层
	QAction* actCopyLayer = createAction(QObject::tr("copy to layer"), ":/ribbon/layer/copy_entity_to_layer.svg");
	connect(actCopyLayer, SIGNAL(triggered()), m_pActionHandler, SLOT(slotCopyToLayer()));

	// 修改图层
	QAction* actRenameLayer = createAction(QObject::tr("rename layer"), ":/ribbon/layer/rename_layer.svg");
	connect(actRenameLayer, SIGNAL(triggered()), m_pActionHandler, SLOT(slotLayersRename()));

	// 下面这排使用 Ribbon 按钮控件，但用自定义等分布局，确保图标尽量铺满且整行平铺。
	QWidget* layerActionRow = new QWidget(allGroup);
	layerActionRow->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

	auto createLayerRibbonButton = [layerActionRow](QAction* action) {
		auto* button = new SARibbonToolButton(layerActionRow);
		button->setAutoRaise(true);
		button->setFocusPolicy(Qt::NoFocus);
		button->setButtonType(SARibbonToolButton::SmallButton);
		button->setToolButtonStyle(Qt::ToolButtonIconOnly);
		button->setDefaultAction(action);
		return button;
	};

	auto* btnOnOff = createLayerRibbonButton(m_pActOnOff);
	auto* btnLock = createLayerRibbonButton(m_pActLock);
	auto* btnNewLayer = createLayerRibbonButton(actNewLayer);
	auto* btnRenameLayer = createLayerRibbonButton(actRenameLayer);
	auto* btnCopyLayer = createLayerRibbonButton(actCopyLayer);

	auto* layerActionLayout = new QHBoxLayout(layerActionRow);
	layerActionLayout->setContentsMargins(0, 0, 0, 0);
	layerActionLayout->setSpacing(0);
	layerActionLayout->addStretch(1);
	layerActionLayout->addWidget(btnOnOff);
	layerActionLayout->addStretch(1);
	layerActionLayout->addWidget(btnLock);
	layerActionLayout->addStretch(1);
	layerActionLayout->addWidget(btnNewLayer);
	layerActionLayout->addStretch(1);
	layerActionLayout->addWidget(btnRenameLayer);
	layerActionLayout->addStretch(1);
	layerActionLayout->addWidget(btnCopyLayer);
	layerActionLayout->addStretch(1);

	allGroup->addWidget(layerActionRow);
	allGroup->setMinimumWidth(200);
	layerPannel->addLargeWidget(allGroup);
}

ComboBoxData* ApplicationWindow::newLayer(DmLayer* layer, QListWidget* plistWidget)
{
	CustomComboboxItem* item = new CustomComboboxItem(plistWidget, layer);
	ComboBoxData* cbxData = initLayerComboboxItem(layer, item, false);
	item->setData(cbxData);
	QListWidgetItem* widgetItem = new QListWidgetItem(plistWidget);
	plistWidget->setItemWidget(widgetItem, item);

	// Reset after selection to avoid a blank area in the layer combo popup.
	connect(m_pLayerTable, QOverload<int>::of(&QComboBox::activated), m_pLayerTable, [this](int) {
		this->m_pLayerTable->view()->verticalScrollBar()->setSliderPosition(0);
	});
	return cbxData;
}

ComboBoxData* ApplicationWindow::initLayerComboboxItem(DmLayer* layer, QWidget* parent, bool isEditBox)
{
	auto docView = m_pCurrentMdiWin->getDocumentView();
	auto document = m_pCurrentMdiWin->getDocument();
	ComboBoxData* data = new ComboBoxData();
	// 显示、隐藏
	data->btnOn = new QToolButton(parent);
	data->setIsOn(!layer->isFrozen());
	connect(data->btnOn, SIGNAL(clicked()), m_pActionHandler, SLOT(slotLayersFreeze()));

	// 锁定、解锁
	data->btnLock = new QToolButton(parent);		
	data->setIsLock(layer->isLocked());
	connect(data->btnLock, SIGNAL(clicked()), m_pActionHandler, SLOT(slotLayersLock()));

	// 打印、不打印
	data->btnPrint = new QToolButton(parent);
	data->setIsPrint(layer->isPrint());
	connect(data->btnPrint, SIGNAL(clicked()), m_pActionHandler, SLOT(slotLayersPrint()));

	// 颜色
	data->btnColor = new QToolButton(parent);
	DmColor layerColor = layer->getPen().getColor();
	data->setColor(QColor(layerColor.red(), layerColor.green(), layerColor.blue(), layerColor.alpha()));
	connect(data->btnColor, SIGNAL(clicked()), m_pActionHandler, SLOT(slotLayersColor()));

	// 名字
	data->labelName = new QPushButton(parent);
	data->labelName->setProperty("isLayerNameButton", true);
	data->labelName->setFlat(true);
	QString layerName = layer->getName();
	data->setLayerName(layerName);
	if (isEditBox)
	{
		QComboBox* cbx = dynamic_cast<QComboBox*>(parent);
		if (cbx)
		{
			connect(data->labelName, &QPushButton::clicked, cbx, [cbx] {
				cbx->showPopup(); // 展开列表
				});
		}
	}
	else
	{
		connect(data->labelName, SIGNAL(clicked()), m_pActionHandler, SLOT(slotLayersActivate()));
	}

	// 删除
	if (!isEditBox)
	{
		data->btnDelete = new QToolButton(parent);
		data->btnDelete->setIcon(QIcon(":/ribbon/layer/delete_layer.svg"));
		connect(data->btnDelete, SIGNAL(clicked()), m_pActionHandler, SLOT(slotLayersDelete()));
	}
	else
	{
		data->btnDelete = nullptr;
	}

	//添加到布局
	auto layout = new  QHBoxLayout(parent);
	layout->addWidget(data->btnOn);
	layout->addWidget(data->btnLock);
	layout->addWidget(data->btnPrint);
	layout->addWidget(data->btnColor);
	layout->addWidget(data->labelName);
	if (isEditBox)
	{
		layout->addStretch();
		layout->setSpacing(0);
		layout->setContentsMargins(2, 1, 20, 0);
	}
	else
	{
		layout->addStretch();
		layout->addWidget(data->btnDelete);
		layout->setSpacing(0);
		layout->setContentsMargins(2, 1, 2, 0);
	}
	parent->setLayout(layout);

	return data;
}

void ApplicationWindow::setDrawingTabName(const QString& fileName)
{
	m_pTabDrawWidget->soltSetDrawingTabName(fileName);
}

int ApplicationWindow::countRow(QPoint p, int row, int height) // 计算鼠标在哪一列和哪一行
{
    if (p.y() < 2)
    {
        return 10 + row;
    }
    else if (p.y() > height - 2)
    {
        return 30 + row;
    }
    else
    {
        return 20 + row;
    }
}

int ApplicationWindow::countLine(QPoint p, int width)
{
    if (p.x() < 2)
    {
        return 1;
    }
    else if (p.x() > width - 2)
    {
        return 3;
    }
    else
    {
        return 2;
    }
}

void ApplicationWindow::enableButtons(const bool enable)
{
	// ribbon按钮
	ribbonBar()->categoryByObjectName("categoryDraw2d")->setEnabled(enable);
	ribbonBar()->categoryByObjectName("categoryOptions")->setEnabled(enable);
	m_pActRedo->setEnabled(enable);
	m_pActUndo->setEnabled(enable);
	for (auto act : ribbonBar()->quickAccessBar()->actions())
	{
		if (act->objectName() == "save-quickbar" || act->objectName() == "saveas-quickbar")
		{
			act->setEnabled(enable);
		}
	}
	auto pannels = ribbonBar()->categoryByObjectName("categoryFile")->pannelList();
	for (auto& pannel : pannels)
	{
		if (pannel->windowTitle() == QObject::tr("File"))
		{
			QList<QAction*> acts = pannel->actions();
			QWidgetAction* wAct = dynamic_cast<QWidgetAction*>(acts.front());
			SARibbonButtonGroupWidget* groupWidget = dynamic_cast<SARibbonButtonGroupWidget*>(wAct->defaultWidget());
			for (auto& act : groupWidget->actions())
			{
				wAct = dynamic_cast<QWidgetAction*>(act);
				QToolButton* btn = dynamic_cast<QToolButton*>(wAct->defaultWidget());
				QString tooltip = btn->toolTip();
				if (tooltip != QObject::tr("open") && tooltip != QObject::tr("new"))
				{
					btn->setEnabled(enable);
				}
			}
		}
		else
		{
			pannel->setEnabled(enable);
		}
	}

	//当前画笔
	if (nullptr != m_pTabDrawWidget)
	{
		m_pTabDrawWidget->getCurrentActivePen()->parentWidget()->setEnabled(enable);
	}

	//命令框
	if (nullptr != m_cmdWin)
	{
		m_cmdWin->getCommandWidget()->setEnabled(enable);
		m_cmdWin->getInfoWidget()->setEnabled(enable);
		m_cmdWin->getTempWidget()->setEnabled(enable);
		//m_cmdWin->getTipWidget()->setEnabled(enable);	这个getTipWidget()会创建
	}

	//状态栏
	if (nullptr != m_pBottomWidget)
	{
		m_pBottomWidget->setEnabled(enable);
	}
}

bool ApplicationWindow::eventFilter(QObject* obj, QEvent* e)
{
	// todo 暂时屏蔽
	//m_editLine->hide();

	auto classname = obj->metaObject()->className();
	QString objName = obj->objectName();
	QEvent::Type eventNow = e->type();
	if (QEvent::MouseMove == eventNow)
	{
		QMouseEvent* evev = dynamic_cast<QMouseEvent*>(e);
		if (classname == QStringLiteral("QWidgetWindow") && objName == "ApplicationWindowClassWindow")
		{			
			m_presentPos = evev->pos();
			QPoint movePoint = m_presentPos;
			QPoint maxPoint(m_presentPos.x() + m_editLine->width() + 10, m_presentPos.y() + m_editLine->height() + 10);
			if (!m_editLine->isHidden())
			{
				if (maxPoint.x() > m_pointVec[6].x() && maxPoint.y() < m_pointVec[6].y())
				{
					m_editLine->move(m_lineWidgetPos.x() + 10, movePoint.y() + 10);
					m_lineWidgetPos.setY(m_presentPos.y());
				}
				else if (maxPoint.y() > m_pointVec[6].y() && maxPoint.x() < m_pointVec[6].x())
				{
					m_editLine->move(movePoint.x() + 10, m_lineWidgetPos.y() + 10);
					m_lineWidgetPos.setX(m_presentPos.x());
				}
				else if (maxPoint.x() < m_pointVec[6].x() && maxPoint.y() < m_pointVec[6].y())
				{
					m_editLine->move(movePoint.x() + 10, movePoint.y() + 10);
					m_lineWidgetPos = m_presentPos;
				}
			}

			if (!pointInPolygon(m_presentPos, m_pointVec))
			{
				m_editLine->hide();
			}
			else
			{
				m_editLine->show();
				//m_cmdWin->m_editline->setFocus();
			}
		}	

		if (m_isPressed)
		{			
			m_isMove = true;
		}
	}
	else if (QEvent::MouseButtonPress == eventNow)
	{
		QMouseEvent* evev = dynamic_cast<QMouseEvent*>(e);
		if (obj != m_pBottomWidget->m_pUnitBtn && classname != QStringLiteral("QWidgetWindow"))
		{
			auto pBottomComboBoxWidget = m_pBottomWidget->getComboBoxWidget();
			if (!pBottomComboBoxWidget->isHidden())
			{
				pBottomComboBoxWidget->hide();
				pBottomComboBoxWidget->lower();
			}
		}

		if (obj != m_pBottomWidget->m_pViewBtn && classname != QStringLiteral("QWidgetWindow"))
		{
			auto viewportsWidget = m_pBottomWidget->getViewportsWidget();
			if (!viewportsWidget->isHidden())
			{
				viewportsWidget->hide();
				viewportsWidget->lower();
			}
		}

		//if (obj != m_pBottomWidget->m_pTheme && classname != QStringLiteral("QWidgetWindow"))
		//{
		//	auto pBottomComboBoxWidget = m_pBottomWidget->getThemeComBoxWidget();
		//	if (!pBottomComboBoxWidget->isHidden())
		//	{
		//		pBottomComboBoxWidget->hide();
		//		pBottomComboBoxWidget->lower();
		//	}
		//}

		if (obj != m_pBottomWidget->m_pSnapModeBtn && classname != QStringLiteral("QWidgetWindow"))
		{
			auto pBottomToolWidget = m_pBottomWidget->getToolWidget();
			if (!pBottomToolWidget->isHidden() && !pointInPolygon(m_presentPos, m_snapWidgetVec))
			{
				pBottomToolWidget->hide();
				pBottomToolWidget->lower();
			}
		}

		if (Qt::LeftButton == evev->button())
		{
			m_isPressed = true;
			m_startMovePos = evev->globalPos();
			m_quadrant = countRow(evev->pos(), countLine(evev->pos(), this->width()), this->height());

			//----------------------------------------------------

		}
		else if (Qt::RightButton == evev->button())
		{
			if (m_pCurrentMdiWin)
			{
				GuiDocumentView* gv = m_pCurrentMdiWin->getDocumentView();
				//gv->deleteRelativeZero();
				GuiEventHandler* handle = gv->getEventHandler();
				int actionNum = handle->getCurrentActionNum();
				if (actionNum == 0)
				{
					m_editLine->hide();
					m_editLine->lower();
					m_cmdWin->getEditline()->setText("");
				}
			}
		}
	}
	else if (QEvent::MouseButtonRelease == eventNow)
	{
		int ry = mapToGlobal(this->pos()).ry();

		//-------------------------------------------------------拖拉至顶部放大--------------------------------------------------------
		if (ry <= 3 && m_startMovePos.y() <= 55 && !this->isMaximized() && m_isMove == true && m_isDoubleClick == false)
		{
			this->showMaximized();
		}
		m_isMove = false;
		m_isPressed = false;
		m_isDoubleClick = false;
	}
	else if (QEvent::MouseButtonDblClick == eventNow)
	{
		QMouseEvent* evev = dynamic_cast<QMouseEvent*>(e);
		if (Qt::LeftButton == evev->button())
		{
			if (classname == QStringLiteral("SARibbonTabBar"))
			{
				return true;
			}
		}
		m_isDoubleClick = true;
	}

	//QEvent::Type eventNow = e->type();
	// 右键隐藏捕捉圆圈
	if (QEvent::MouseButtonPress == eventNow)
	{
		QMouseEvent* evev = dynamic_cast<QMouseEvent*>(e);
		if (Qt::RightButton == evev->button())
		{
			if (m_pCurrentMdiWin)
			{
				GuiDocumentView* gv = m_pCurrentMdiWin->getDocumentView();
				gv->hideRelativeZero(false);
			}
		}
	}

	return SARibbonMainWindow::eventFilter(obj, e);
}

void ApplicationWindow::updateUndoRedo()
{
    if (nullptr == m_pCurrentMdiWin)
    {
        return;
    }

    DmDocument* document = m_pCurrentMdiWin->getDocument();
    bool hasUndo = false;
    std::string undoName;
    bool hasRedo = false;
    std::string redoName;
    document->getCmdData(hasUndo, undoName, hasRedo, redoName);
    if (hasUndo)
    {
        m_pActUndo->setEnabled(true);
        m_pActUndo->setText(QObject::tr("Undo") + ": " + QString::fromStdString(undoName));
    }
    else
    {
        m_pActUndo->setEnabled(false);
        m_pActUndo->setText("");
    }
    if (hasRedo)
    {
        m_pActRedo->setEnabled(true);
        m_pActRedo->setText(QObject::tr("Redo") + ": " + QString::fromStdString(redoName));
    }
    else
    {
        m_pActRedo->setEnabled(false);
        m_pActRedo->setText("");
    }
}
