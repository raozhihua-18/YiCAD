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

/// @file ApplicationWindow.h
/// @brief 应用程序主窗口类，管理Ribbon界面、绘图区域和图层面板

#ifndef APPLICATIONWINDOW_H
#define APPLICATIONWINDOW_H

#include "SARibbonMainWindow.h"
#include <QColorDialog>
#include <QScrollBar>
#include <set>
#include <map>
#include <memory>

class SARibbonCategory;
class SARibbonContextCategory;
class SARibbonCustomizeWidget;
class SARibbonActionsManager;
class SARibbonQuickAccessBar;
class SARibbonButtonGroupWidget;
class QTextEdit;
class SARibbonPannel;
class SARibbonComboBox;
struct ComboBoxData;
class DmLayer;
class QListWidget;
class CustomComboboxItem;

class DmDocument;
class QMdiArea;
class MDIWindow;
class GuiDocumentView;
class UIDialogFactory;

class UIActionHandler;
class UIWindowSize;
class UIBottomWindow;
class UITabDrawWidget;
class UIActionGroupManager;
class UICommandWidget;
class UIBlockListWidget;
class UIBlockSaveAs;
class AIAssistant;
class ApplicationPluginHostContext;
class HostApi;
class PluginManager;
class PluginRegistry;
class PluginUiAdapter;
struct SingleTabDraw;

/// @brief 应用程序主窗口，继承自SARibbonMainWindow，管理Ribbon菜单、MDI绘图区域和图层面板
class ApplicationWindow : public SARibbonMainWindow
{
    Q_OBJECT
public:
    ApplicationWindow(QWidget* par = nullptr);
    ~ApplicationWindow();

    /// @brief 判断点是否在多边形内（射线法）
    /// @param [in] point 待检测的点
    /// @param [in] vec 多边形顶点序列
    /// @return true 如果点在多边形内部
    bool pointInPolygon(const QPoint point, const std::vector<QPoint> vec);

    /// @brief 获取应用程序窗口单例
    /// @return 应用程序窗口指针
    static ApplicationWindow* getAppWindow();

    /// @brief 获取重做QAction
    /// @return 重做QAction指针
    QAction* getRedoAction();

    /// @brief 获取撤销QAction
    /// @return 撤销QAction指针
    QAction* getUndoAction();

    /// @brief 设置重做是否可用
    /// @param [in] enable true=可用
    void setRedoEnable(bool enable);

    /// @brief 设置撤销是否可用
    /// @param [in] enable true=可用
    void setUndoEnable(bool enable);

    /// @brief 获取MDI区域（const版本）
    /// @return MDI区域指针
    QMdiArea const* getMdiArea() const;

    /// @brief 获取MDI区域
    /// @return MDI区域指针
    QMdiArea* getMdiArea();

    /// @brief 获取当前MDI窗口（const版本）
    /// @return MDI窗口指针
    const MDIWindow* getMDIWindow() const;

    /// @brief 获取当前MDI窗口
    /// @return MDI窗口指针
    MDIWindow* getMDIWindow();

    /// @brief 获取当前文档视图（const版本）
    /// @return 文档视图指针
    const GuiDocumentView* getDocumentView() const;

    /// @brief 获取当前文档视图
    /// @return 文档视图指针
    GuiDocumentView* getDocumentView();

    /// @brief 获取当前文档（const版本）
    /// @return 文档指针
    const DmDocument* getDocument() const;

    /// @brief 获取当前文档
    /// @return 文档指针
    DmDocument* getDocument();

    /// @brief 获取动作处理器
    /// @return 动作处理器指针
    UIActionHandler* getActionHandler() const;

    /// @brief 获取绘图选项卡控件
    /// @return 选项卡控件指针
    UITabDrawWidget* getTabDrawWidget() const;

    /// @brief 获取所有文档列表
    /// @return 文档指针向量
    std::vector<DmDocument*> getDocuments() const;

    /// @brief 获取命令窗口
    /// @return 命令窗口指针
    UICommandWidget* getCmdWidget() const;

    /// @brief 获取底部状态栏窗口
    /// @return 底部窗口指针
    UIBottomWindow* getBottomWidget() const;

    /// @brief 根据文档获取对应的MDI窗口
    /// @param [in] doc 文档指针
    /// @return MDI窗口指针
    MDIWindow* getWindowWithDoc(const DmDocument* doc);

    /// @brief 获得图层下拉列表框
    /// @return 图层下拉框指针
    SARibbonComboBox* getLayerCombox() const;

    /// @brief 获得图层下拉列表框绑定的列表
    /// @return 图层列表控件指针
    QListWidget* getLayerTableWidget() const;

    /// @brief 获取当前选中的图层项数据
    /// @return 图层下拉项数据指针
    ComboBoxData* getCurrentLayerItem() const;

    /// @brief 获取"显示所有图层"QAction
    /// @return QAction指针
    QAction* getActOnOff() const;

    /// @brief 获取"解锁所有图层"QAction
    /// @return QAction指针
    QAction* getActLock() const;

    /// @brief 获取图层下拉框所有项
    /// @return 图层下拉项指针向量
    std::vector< CustomComboboxItem*> getLayerComboboxItems();

    /// @brief 创建单个图层下拉项
    /// @param [in] itemData 图层数据
    /// @param [in] plistWidget 列表控件
    /// @return 图层下拉项数据
    ComboBoxData* newLayer(DmLayer* itemData, QListWidget* plistWidget);

    /// @brief 创建图层下拉列表的一项，或用于创建图层下拉列表的当前项
    /// @param [in] layer 图层对象
    /// @param [in] parent 父控件
    /// @param [in] isEditBox 是否为编辑框模式
    /// @return 图层下拉项数据
    ComboBoxData* initLayerComboboxItem(DmLayer* layer, QWidget* parent, bool isEditBox);

    /// @brief 设置当前图纸选项卡名称和toolTip
    /// @param [in] fileName 文件名
    void setDrawingTabName(const QString& fileName);

private:
    void createCategoryFile(SARibbonCategory* page);
    void createCategoryOptions(SARibbonCategory* page);
    void createCategoryDraw2d(SARibbonCategory* page);
    void createCategorySolver(SARibbonCategory* page);
    void createQuickAccessBar(SARibbonQuickAccessBar* quickAccessBar);
    QAction* createAction(const QString& text, const QString& iconurl, const QString& objName);
    QAction* createAction(const QString& text, const QString& iconurl);

    /// @brief 创建图层列表
    void createLayerTable(SARibbonPannel* layerPannel);

    /// @brief 计算鼠标所在行区域
    /// @param [in] p 鼠标位置
    /// @param [in] row 基准行号
    /// @param [in] height 区域高度
    /// @return 编码后的区域值
    int countRow(QPoint p, int row, int height);

    /// @brief 计算鼠标所在列区域
    /// @param [in] p 鼠标位置
    /// @param [in] width 区域宽度
    /// @return 编码后的区域值
    int countLine(QPoint p, int width);

    /// @brief 设置UI是否可用（除"打开""新建"）
    /// @param [in] enable true=启用
    void enableButtons(const bool enable);

    /// @brief 鼠标跟随框事件过滤器
    bool eventFilter(QObject* obj, QEvent* e);

private slots:
    void onStyleClicked(int id);
    void onActionCustomizeTriggered(bool b);
    void onActionCustomizeAndSaveTriggered(bool b);
    void onActionRemoveAppBtnTriggered(bool b);
    void onActionUseQssTriggered();
    void onActionLoadCustomizeXmlFileTriggered();
    void onActionWindowFlagNormalButtonTriggered(bool b);


    /// @brief 窗体关闭事件
    /// @param [in] event 关闭事件
    void closeEvent(QCloseEvent* event);

    /// @brief 设置快捷键
    /// @param [in] e 键盘事件
    virtual void keyPressEvent(QKeyEvent* e) override;

    /// @brief 键盘回车事件
    void slotEnter();

    /// @brief 键盘删除事件
    void slotDelete();

    /// @brief 主窗体放缩事件
    /// @param [in] event 放缩事件
    void resizeEvent(QResizeEvent* event);

public slots:
    /// @brief 键盘ESC事件，取消所有操作
    void slotKillAllActions();

    /// @brief 更新图层列表
    void updateLayerTable();

    /// @brief 更新当前画笔控件
    void updateCurrentPenWidget();

    /// @brief 更新undo/redo按钮
    void updateUndoRedo();

    /// @brief 选项卡切换事件
    void slotsTabChangeEvent();

private:
    SARibbonBar*                    m_pRibbon = nullptr;                ///< Ribbon菜单条基类
    SARibbonCustomizeWidget*        m_customizeWidget = nullptr;        ///< 自定义控件
    SARibbonActionsManager*         m_actMgr = nullptr;                 ///< 动作管理器

    UICommandWidget*                m_cmdWin = nullptr;                 ///< 命令输入框
    MDIWindow*                      m_pCurrentMdiWin = nullptr;         ///< 当前绘图区域
    QMdiArea*                       m_pDrawingArea = nullptr;           ///< 绘图区域底板
    UIActionHandler*                m_pActionHandler = nullptr;         ///< 按钮点击事件执行函数类
    UITabDrawWidget*                m_pTabDrawWidget = nullptr;         ///< 图纸选项卡对象
    UIBottomWindow*                 m_pBottomWidget = nullptr;          ///< 底部状态栏对象

    UIDialogFactory*                m_pDialogFactory = nullptr;         ///< 交互过程中显示的设置bar

    QPoint                          m_presentPos;                       ///< 鼠标当前位置
    QPoint                          m_lineWidgetPos;                    ///< 命令行控件位置
    QWidget*                        m_editLine = nullptr;               ///< 命令行临时编辑控件
    std::vector<QPoint>             m_pointVec;                         ///< 鼠标安全区域多边形顶点
    std::vector<QPoint>             m_snapWidgetVec;                    ///< 捕捉控件区域多边形顶点

    QAction*                        m_pActUndo = nullptr;               ///< 撤销
    QAction*                        m_pActRedo = nullptr;               ///< 重做

    bool                            m_isPressed = false;                ///< 鼠标是否按下
    bool                            m_isMove = false;                   ///< 鼠标是否移动
    bool                            m_isDoubleClick = false;            ///< 是否双击
    QPoint                          m_startMovePos;                     ///< 鼠标按下起始位置
    int                             m_quadrant = 0;                     ///< 窗口象限区域

    // 图层列表UI组件
    SARibbonComboBox*               m_pLayerTable = nullptr;            ///< 图层下拉框
    QListWidget*                    m_pLayerTableWidget = nullptr;      ///< 图层下拉列表
    ComboBoxData*                   m_pCurrentLayerItem = nullptr;      ///< 当前选中项
    QAction*                        m_pActOnOff = nullptr;              ///< 显示所有图层
    QAction*                        m_pActLock = nullptr;               ///< 解锁所有图层

    static ApplicationWindow*       appWindow;                          ///< 应用程序窗口单例

    QWidget*                        m_pLibraryList = nullptr;           ///< 库列表控件

    // 视图列表UI组件
    SARibbonComboBox*               m_pViewportTable = nullptr;         ///< 视图下拉框
    QListWidget*                    m_pViewportWidget = nullptr;        ///< 视图下拉列表

    // AI 助手
    QAction*                        m_pActAI = nullptr;                 ///< AI助手按钮Action
    AIAssistant*                    m_pAIAssistant = nullptr;           ///< AI 助手控制器

    /// @brief 新插件运行时；声明顺序保证 Manager 最先析构，宿主上下文最后析构。
    std::unique_ptr<ApplicationPluginHostContext> m_pluginHostContext;
    std::unique_ptr<PluginRegistry>                m_pluginRegistry;
    std::unique_ptr<HostApi>                       m_pluginHostApi;
    std::unique_ptr<PluginUiAdapter>               m_pluginUiAdapter;
    std::unique_ptr<PluginManager>                 m_pluginManager;
};

#endif  // APPLICATIONWINDOW_H
