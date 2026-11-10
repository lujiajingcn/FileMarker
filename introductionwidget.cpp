// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "introductionwidget.h"

#include <QEvent>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QHBoxLayout>
#include <QPainter>
#include <QToolBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QtMath>
#include <QTextDocument>
#include <limits>
#include <cmath>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>


IntroductionWidget::IntroductionWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
    parentWidget()->installEventFilter(this);

    QPalette p = palette();
    p.setColor(QPalette::WindowText, QColor(220, 220, 220));
    setPalette(p);

    m_textWidget = new QWidget(this);
    auto layout = new QVBoxLayout;
    // 给正文块一个不透明的深色底：半透明遮罩下层的文件列表还会透出来，
    // 浅灰正文压在白底文件名上几乎看不清。
    m_textWidget->setAttribute(Qt::WA_StyledBackground, true);
    m_textWidget->setStyleSheet(QStringLiteral(
        "background-color: #121212; border-radius: 10px;"));
    // 样式表在纯 QWidget 上偶尔不生效（取决于平台样式），再补一层调色板底色兜底，
    // 保证正文块永远是不透明的，不会和底下的文件列表糊在一起。
    m_textWidget->setAutoFillBackground(true);
    {
        QPalette textPal = m_textWidget->palette();
        textPal.setColor(QPalette::Window, QColor(18, 18, 18));
        m_textWidget->setPalette(textPal);
    }
    // 正文与页码两个 QLabel 用透明底，避免各自刷一块颜色把圆角盖住
    layout->setContentsMargins(22, 18, 22, 14);
    layout->setSpacing(8);
    m_textWidget->setLayout(layout);

    m_stepText = new QLabel(this);
    m_stepText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stepText->setWordWrap(true);
    m_stepText->setTextFormat(Qt::RichText);
    {
        QPalette labelPal = m_stepText->palette();
        labelPal.setColor(QPalette::WindowText, QColor(220, 220, 220));
        m_stepText->setPalette(labelPal);
    }
    m_stepText->setOpenExternalLinks(true);
    m_stepText->installEventFilter(this);
    layout->addWidget(m_stepText);

    m_continueLabel = new QLabel(this);
    m_continueLabel->setAlignment(Qt::AlignCenter);
    m_continueLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_continueLabel->setWordWrap(true);
    auto fnt = font();
    fnt.setPointSizeF(fnt.pointSizeF() * 1.5);
    m_continueLabel->setFont(fnt);
    m_continueLabel->setPalette(palette());
    layout->addWidget(m_continueLabel);

    // 底部导航按钮：跳过 / 上一步 / 下一步
    m_buttonLayout = new QHBoxLayout;
    m_btnSkip = new QPushButton(tr("跳过"));
    m_btnPrev = new QPushButton(tr("上一步"));
    m_btnNext = new QPushButton(tr("下一步"));
    for (QPushButton *btn : {m_btnSkip, m_btnPrev, m_btnNext}) {
        // 正文块是深色底，按钮默认的浅灰文字会糊在背景里，这里统一成浅色描边样式
        btn->setAutoDefault(false);
        btn->setStyleSheet(QStringLiteral(
            "QPushButton { color: #E6E6E6; background-color: rgba(255,255,255,0.10);"
            " border: 1px solid rgba(255,255,255,0.35); border-radius: 6px;"
            " padding: 4px 16px; }"
            "QPushButton:hover { background-color: rgba(255,255,255,0.20); }"
            "QPushButton:disabled { color: rgba(230,230,230,0.40);"
            " border-color: rgba(255,255,255,0.15); }"));
    }
    m_buttonLayout->addWidget(m_btnSkip);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_btnPrev);
    m_buttonLayout->addWidget(m_btnNext);
    layout->addLayout(m_buttonLayout);

    connect(m_btnSkip, &QPushButton::clicked, this, &IntroductionWidget::finish);
    connect(m_btnPrev, &QPushButton::clicked, this, [this]() {
        if (m_step > 0)
            setStep(m_step - 1);
    });
    connect(m_btnNext, &QPushButton::clicked, this, &IntroductionWidget::step);

    m_bodyCss = "font-size: 16px; line-height: 150%;";
    m_items = {
        {QLatin1String("dwLabels"),
         tr("标签管理"),
         tr("通过树形结构来对标签进行展示和管理"),
         tr("<ul>"
            "<li>本面板自己的工具条用于管理标签：新建标签、新建子标签、删除标签、上移/下移/左移/右移。</li>"
            "<li>标签之间可以任意嵌套成树，也可以保持成简单列表。</li>"
            "<li>每次改动都会自动保存到 labels.xml，下次启动时自动读取。</li>"
            "<li>点击可以选中一个标签，按住 Ctrl 或 Shift 可以多选。</li>"
            "<li>给文件打标签、按标签查找文件，用的都是\"当前选中的标签\"。</li>"
            "<li>也可以把标签从本面板直接拖到文件上：拖到某个文件=只给它加，拖到空白处=给当前选中的文件批量加。</li>"
            "<li>右键菜单还有：重命名、删除标签，以及\"同时把改名/删除同步到文件上的标签\"等选项。</li>"
            "</ul>")},
        {QLatin1String("dwCurDirLabels"),
         tr("当前目录标签"),
         tr("展示当前浏览目录中文件的所有标签"),
         tr("<ul>"
            "<li>进入某个目录后会自动扫描，列出该目录下所有文件用到的标签（不需要先执行\"遍历文件\"）。</li>"
            "<li>点击这里的标签，等同于在标签库里选中它，可以直接用于查找文件。</li>"
            "<li>切换浏览目录时，列表会自动更新。</li>"
            "</ul>")},
        {QLatin1String("centralWidget"),
         tr("文件浏览"),
         tr("模拟 Windows 文件管理器，用于查看和管理文件"),
         tr("<ul>"
            "<li>分为两个部分：左侧的目录导航栏和右侧的文件显示区。</li>"
            "<li>两种视图：详细列表和缩略图，点右上角的\"详情显示 / 缩略图显示\"切换，右键菜单里也能切。</li>"
            "<li>可以点击选中单个文件或文件夹，也可按住 Ctrl 或 Shift 选中多个。</li>"
            "<li>文件显示区选中文件 + 标签管理区选中标签，即可通过工具栏为文件添加标签。</li>"
            "<li>双击标签单元格（两种视图都可以）会弹出\"增删式\"标签对话框；标签上的 ✕ 直接删掉这个标签。</li>"
            "<li>双击文件夹可进入该目录，双击文件会使用默认程序打开；重命名按 F2 或右键。</li>"
            "<li>右侧的标签列是一行行带 ✕ 的小药丸（chip），多了会自动换行。</li>"
            "</ul>")},
        {QLatin1String("fileCommandBar"),
         tr("工具栏"),
         tr("根据选中的文件和标签执行各种操作"),
         tr("<p style=\"margin-top: 20px\">各个按钮的说明如下：<table>"
             "<tr><td style=\"padding-right: 20px\">AI自动生成标签:</td><td>调用 AI 分析选中文件并自动生成标签。</td></tr>"
             "<tr><td style=\"padding-right: 20px\">添加标签:</td><td>根据标签区选中的标签为文件查看区选中的文件添加标签。</td></tr>"
             "<tr><td style=\"padding-right: 20px\">清空标签:</td><td>清空选中文件的全部标签（会先确认，可用 Ctrl+Z 撤销）。</td></tr>"
             "<tr><td style=\"padding-right: 20px\">查找文件:</td><td>根据选中的标签，在选中的文件夹中查找匹配的文件。</td></tr>"
             "<tr><td style=\"padding-right: 20px\">遍历文件:</td><td>遍历选中的文件夹，把标签信息写入缓存数据库，加速后续查找。</td></tr>"
             "<tr><td style=\"padding-right: 20px\">查找条件:</td><td>点搜索按钮右侧的小箭头（或按 F3），选择标签之间的逻辑关系（与/或）和查找数据来源（缓存/磁盘）。</td></tr>"
             "<tr><td>帮助菜单:</td><td>界面介绍（就是本向导）、作者信息、检查 AI 打标环境。</td></tr>"
             "</table></p>")},
        {QLatin1String("menuBar"),
         tr("查找结果与标签备份"),
         tr("结果页怎么看、标签怎么备份，以及常用快捷键"),
         tr("<ul>"
             "<li>查找结果页顶部会写明搜索条件、来源（缓存 / 磁盘扫描），以及这份缓存是多久之前遍历的。</li>"
             "<li>对结果没把握时点【重新扫描】：忽略缓存，直接按同样的条件重新查磁盘。</li>"
             "<li>标签存在 NTFS 数据流里：文件拷到 U 盘(FAT32)、压成 zip、传网盘时会丢失，"
             "跨机器/跨介质前请先用【文件 → 导出标签…】存一份 JSON，需要时再【导入标签…】写回。</li>"
             "<li>导入时如果文件路径变了，可以指定新的根目录，程序会按备份里的相对路径找回。</li>"
             "<li>常用快捷键：Ctrl+F 查找、F3 查找条件、Ctrl+A 全选、Enter 打开、F2 重命名、"
             "Delete / Ctrl+Shift+C 清空标签、Ctrl+Z 撤销。</li>"
             "</ul>")},
        {QLatin1String("statusBar"),
         tr("状态栏"),
         tr("显示当前操作的状态和进度信息"),
         tr("<ul>"
             "<li>在执行查找、遍历、AI 生成等耗时操作时，状态栏会显示进度反馈。</li>"
             "<li>操作完成后会显示结果摘要信息。</li>"
             "</ul>")},
        {{},
         tr("结束"),
         tr("感谢使用 FileMarker！"),
         tr("<p>项目源码：<a style=\"color: #41CD52\" "
            "href=\"https://github.com/lujiajingcn/FileMarker\">FileMarker</a></p>"
            "<p>如有问题或建议，欢迎在 GitHub 上提交 Issue。</p>")}};
    setStep(0);
    resizeToParent();
}

IntroductionWidget::~IntroductionWidget()
{
    if (m_tempSpotlightWidget) {
        m_tempSpotlightWidget->setParent(nullptr);
        delete m_tempSpotlightWidget;
    }
}

bool IntroductionWidget::event(QEvent *e)
{
    if (e->type() == QEvent::ShortcutOverride) {
        e->accept();
        return true;
    }
    return QWidget::event(e);
}

bool IntroductionWidget::eventFilter(QObject *obj, QEvent *ev)
{
    if (obj == parent() && ev->type() == QEvent::Resize)
        resizeToParent();
    else if (obj == m_stepText && ev->type() == QEvent::MouseButtonRelease)
        step();
    return QWidget::eventFilter(obj, ev);
}

const int SPOTLIGHT_INSET_X = 4;   // 聚光灯相对目标左右各外扩多少
const int SPOTLIGHT_INSET_Y = 2;   // 上下各外扩多少


/**
 * 在锚点外侧画一小段指向锚点的箭头。
 *
 * 箭头只走水平或垂直，不画斜线：斜箭头在视觉上显得歪斜、和界面网格也对不齐。
 *   目标在左/右  -> 水平箭头，落在目标的左边缘或右边缘外侧；
 *   目标在上/下  -> 竖直箭头，落在目标的上边缘或下边缘外侧。
 * 箭头贴着控件、长度固定，不跨越整屏空白。
 */
void IntroductionWidget::drawPointerArrow(QPainter &painter, const QRect &anchorRect)
{
    const int arrowLength = 42;   // 箭头总长
    const int gap = 14;           // 箭尖与控件外沿的距离（留出箭头头部的空间）
    const int clampMargin = 16;   // 箭头沿边落点时，离控件拐角的最小距离

    const QRect textRect = m_textWidget->geometry();
    const QPoint textCenter = textRect.center();
    if (anchorRect.contains(textCenter)) {
        // 正文块本身就在高亮区里（例如整个文件区），没有可指的方向
        return;
    }

    const QPoint anchorCenter = anchorRect.center();
    const int dx = anchorCenter.x() - textCenter.x();
    const int dy = anchorCenter.y() - textCenter.y();

    QPoint tip;        // 箭尖
    QPointF unit;      // 从箭尖指向正文块的方向（箭头指向的反方向）
    if (qAbs(dx) >= qAbs(dy)) {
        const int y = qBound(anchorRect.top() + clampMargin,
                             textCenter.y(),
                             anchorRect.bottom() - clampMargin);
        if (dx > 0) {
            tip = QPoint(anchorRect.left() - gap, y);
            unit = QPointF(-1.0, 0.0);
        } else {
            tip = QPoint(anchorRect.right() + gap, y);
            unit = QPointF(1.0, 0.0);
        }
    } else {
        const int x = qBound(anchorRect.left() + clampMargin,
                             textCenter.x(),
                             anchorRect.right() - clampMargin);
        if (dy > 0) {
            tip = QPoint(x, anchorRect.top() - gap);
            unit = QPointF(0.0, -1.0);
        } else {
            tip = QPoint(x, anchorRect.bottom() + gap);
            unit = QPointF(0.0, 1.0);
        }
    }

    const QPointF tail = QPointF(tip) + unit * arrowLength;

    painter.save();
    painter.setOpacity(1.0);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QColor color(65, 205, 82);   // 与 Qt Creator 的 tour 相同的绿色
    painter.setPen(QPen(QBrush(color), 4, Qt::SolidLine, Qt::RoundCap, Qt::MiterJoin));
    painter.drawLine(tail, QPointF(tip));

    // 箭头头部：两条斜线，张开约 90 度
    const qreal head = 15.0;
    const qreal dirAngle = std::atan2(unit.y(), unit.x());   // 箭头指向
    for (int nSide = -1; nSide <= 1; nSide += 2) {
        const qreal a = dirAngle + nSide * M_PI / 4.0;
        painter.drawLine(QPointF(tip),
                         QPointF(tip.x() + head * std::cos(a),
                                 tip.y() + head * std::sin(a)));
    }

    painter.restore();
}

namespace {

/**
 * 条状控件里真正有内容的区域。
 *
 * 工具条/菜单栏本身是整条铺满窗口的，但它们的内容（按钮、菜单项）只占左边一小段；
 * 聚光灯如果直接按控件矩形画，右边一大片空白也会被点亮，看着像是照亮了半屏。
 * 这里把每个按钮/菜单项的矩形并起来，作为真正的"目标区域"。
 */
QRect barContentRect(QWidget *bar, QWidget *coordinateParent)
{
    if (bar == nullptr || coordinateParent == nullptr)
        return QRect();

    QRect content;
    if (auto *pToolBar = qobject_cast<QToolBar *>(bar)) {
        const QList<QAction *> qLActions = pToolBar->actions();
        for (QAction *pAction : qLActions) {
            QWidget *pWidget = pToolBar->widgetForAction(pAction);
            if (pWidget == nullptr || !pWidget->isVisible())
                continue;
            const QRect r(pWidget->mapTo(coordinateParent, QPoint(0, 0)), pWidget->size());
            content = content.isNull() ? r : content.united(r);
        }
    } else if (auto *pMenuBar = qobject_cast<QMenuBar *>(bar)) {
        const QList<QAction *> qLActions = pMenuBar->actions();
        for (QAction *pAction : qLActions) {
            if (!pAction->isVisible())
                continue;
            const QRect r = pMenuBar->actionGeometry(pAction);
            if (r.isNull())
                continue;
            const QRect mapped(r.topLeft() + pMenuBar->mapTo(coordinateParent, QPoint(0, 0)),
                               r.size());
            content = content.isNull() ? mapped : content.united(mapped);
        }
    }

    return content.isNull() ? QRect() : content;
}

/** 目标是不是"条状"控件：工具栏 / 菜单栏 / 状态栏，或者特别矮的控件 */
bool isBarLikeTarget(const QPointer<QWidget> &anchor)
{
    if (anchor.isNull())
        return false;
    QWidget *w = anchor.data();
    return qobject_cast<QToolBar *>(w) != nullptr
           || qobject_cast<QMenuBar *>(w) != nullptr
           || qobject_cast<QStatusBar *>(w) != nullptr
           || w->height() <= 40;
}

}   // namespace

void IntroductionWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setOpacity(.87);
    const QColor backgroundColor = Qt::black;
    if (m_stepPointerAnchor) {
        const QPoint anchorPos = m_stepPointerAnchor->mapTo(parentWidget(), QPoint{0, 0});
        const QRect anchorRect(anchorPos, m_stepPointerAnchor->size());
        // 工具栏/菜单栏这类控件是"整条"长条，但真正有内容的只有左边一小段，
        // 直接按控件矩形照亮会把右边大片空白也点亮，所以这类目标改用"内容区域"。
        // 状态栏这种取不到子项内容的，仍然退回控件自身的矩形，不能退化成空矩形
        // （空矩形加 18px 外扩会跑到窗口左上角，第 6 页之前就是这样）。
        QRect targetRect = anchorRect;
        if (isBarLikeTarget(m_stepPointerAnchor)) {
            const QRect contentRect = barContentRect(m_stepPointerAnchor, parentWidget());
            if (!contentRect.isEmpty())
                targetRect = contentRect;
        }
        // 所有页面统一：聚光灯紧贴目标（左右 4px、上下 2px），不做大范围外扩。
        // 外扩 18px 会在目标周围糊出一大圈亮边，和"只框住这一处"的预期不符。
        const QRect spotlightRect = targetRect.adjusted(-SPOTLIGHT_INSET_X,
                                                        -SPOTLIGHT_INSET_Y,
                                                        SPOTLIGHT_INSET_X,
                                                        SPOTLIGHT_INSET_Y);

        // darken the background to create a spotlighted area
        if (spotlightRect.left() > 0) {
            p.fillRect(0, 0, spotlightRect.left(), height(), backgroundColor);
        }
        if (spotlightRect.top() > 0) {
            p.fillRect(spotlightRect.left(),
                       0,
                       width() - spotlightRect.left(),
                       spotlightRect.top(),
                       backgroundColor);
        }
        if (spotlightRect.right() < width() - 1) {
            p.fillRect(spotlightRect.right() + 1,
                       spotlightRect.top(),
                       width() - spotlightRect.right() - 1,
                       height() - spotlightRect.top(),
                       backgroundColor);
        }
        if (spotlightRect.bottom() < height() - 1) {
            p.fillRect(spotlightRect.left(),
                       spotlightRect.bottom() + 1,
                       spotlightRect.width(),
                       height() - spotlightRect.bottom() - 1,
                       backgroundColor);
        }

        // 高亮区外沿画一圈细绿线：比圆角边框图更贴合"紧贴目标"的小范围
        p.setOpacity(0.55);
        p.setPen(QPen(QColor(65, 205, 82), 1));
        p.setBrush(Qt::NoBrush);
        p.drawRect(spotlightRect.adjusted(0, 0, -1, -1));
        p.setOpacity(0.87);

        // 目标旁边画一小段箭头，指向被高亮的控件（参考 Qt Creator 的 UI tour）：
        // 箭头贴着控件外侧、长度固定，不跨越整屏空白。
        drawPointerArrow(p, targetRect);
    } else {
        p.fillRect(rect(), backgroundColor);
    }
}

void IntroductionWidget::keyPressEvent(QKeyEvent *ke)
{
    if (ke->key() == Qt::Key_Escape)
        finish();
    else if ((ke->modifiers()
              & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier))
             == Qt::NoModifier) {
        const Qt::Key backKey = QGuiApplication::isLeftToRight() ? Qt::Key_Left : Qt::Key_Right;
        if (ke->key() == backKey) {
            if (m_step > 0)
                setStep(m_step - 1);
        } else {
            step();
        }
    }
}

void IntroductionWidget::mouseReleaseEvent(QMouseEvent *me)
{
    me->accept();
    step();
}

void IntroductionWidget::finish()
{
    if (m_tempSpotlightWidget) {
        m_tempSpotlightWidget->setParent(nullptr);
        delete m_tempSpotlightWidget;
        m_tempSpotlightWidget.clear();
    }
    hide();
    deleteLater();
}

void IntroductionWidget::step()
{
    if (m_step >= m_items.size() - 1)
        finish();
    else
        setStep(m_step + 1);
}

void IntroductionWidget::setStep(uint index)
{
    if(index >= m_items.size())
    {
        return;
    }
    m_step = index;
    m_continueLabel->setText(tr("界面介绍 %1/%2").arg(m_step + 1).arg(m_items.size()));
    const Item &item = m_items.at(m_step);
    m_stepText->setText("<html><body style=\"" + m_bodyCss + "\">" + "<h1>" + item.title
                        + "</h1><p>" + item.brief + "</p>" + item.description + "</body></html>");

    // 清理上一步的临时 spotlight widget
    if (m_tempSpotlightWidget) {
        m_tempSpotlightWidget->setParent(nullptr);
        delete m_tempSpotlightWidget;
        m_tempSpotlightWidget.clear();
    }
    m_stepPointerAnchor.clear();

    const QString anchorObjectName = m_items.at(m_step).pointerAnchorObjectName;
    if (!anchorObjectName.isEmpty()) {
        m_stepPointerAnchor = parentWidget()->findChild<QWidget *>(anchorObjectName);
    }

    updateButtonStates();
    calTextGeometry();
    update();
}

void IntroductionWidget::updateButtonStates()
{
    m_btnPrev->setEnabled(m_step > 0);
    if (m_step >= m_items.size() - 1) {
        m_btnNext->setText(tr("完成"));
        m_btnSkip->setVisible(false);   // 已经在最后一页了，没有可跳过的内容
    } else {
        m_btnNext->setText(tr("下一步"));
        m_btnSkip->setVisible(true);
    }
}

void IntroductionWidget::resizeToParent()
{
    if(parentWidget() == nullptr)
    {
        return;
    }
    setGeometry(QRect(QPoint(0, 0), parentWidget()->size()));
    calTextGeometry();
}

/**
 * 正文块在所有页面用同一套尺寸与位置。
 *
 * 参考 Qt Creator 的 UI tour：翻页时文字框不移动、也不缩放，
 * 用户只需要读内容、看箭头，不用重新寻找版面。
 * 高度取"最长的一页"并缓存，只有窗口宽度变化时才重新测量。
 */
void IntroductionWidget::calTextGeometry()
{
    if (parentWidget() == nullptr || m_stepText == nullptr)
        return;

    const int margin = 24;
    // 宽度：窗口的 46%，限制在 [540, 700] 之间
    const int textWidth = qBound(540, int(width() * 0.46), 700);
    const int innerWidth = qMax(200, textWidth - 44);   // 去掉左右内边距后的正文宽度

    if (m_nMeasureWidth != innerWidth) {
        int nMaxHeight = 0;
        for (const Item &item : m_items)
            nMaxHeight = qMax(nMaxHeight, measurePageHeight(item, innerWidth));
        // 再留出页码、内边距和行距的位置
        m_nTextBlockHeight = qBound(320, nMaxHeight + 96, int(height() * 0.72));
        m_nMeasureWidth = innerWidth;
    }

    const int left = (width() - textWidth) / 2;
    const int top = qMax(margin, (height() - m_nTextBlockHeight) / 2);
    m_textWidget->setGeometry(QRect(left, top, textWidth, m_nTextBlockHeight));
}

/** 用 QTextDocument 量一页正文需要多高（与真实渲染一致，避免翻页时框体跳动） */
int IntroductionWidget::measurePageHeight(const Item &item, int textWidth) const
{
    const QString html = "<html><body style=\"" + m_bodyCss + "\">" + "<h1>" + item.title
                         + "</h1><p>" + item.brief + "</p>" + item.description + "</body></html>";

    QTextDocument doc;
    doc.setDefaultFont(m_stepText->font());
    doc.setTextWidth(textWidth);
    doc.setHtml(html);
    return qCeil(doc.size().height());
}
