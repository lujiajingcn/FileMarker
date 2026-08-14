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
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

void drawCornerImage(const QImage &img, QPainter *painter, const QRect &rect,
                                  int left, int top, int right, int bottom)
{
    // source rect for drawImage() calls needs to be specified in DIP unit of the image
    const qreal imagePixelRatio = img.devicePixelRatio();
    const qreal leftDIP = left * imagePixelRatio;
    const qreal topDIP = top * imagePixelRatio;
    const qreal rightDIP = right * imagePixelRatio;
    const qreal bottomDIP = bottom * imagePixelRatio;

    const QSize size = img.size();
    if (top > 0) { //top
        painter->drawImage(QRectF(rect.left() + left, rect.top(), rect.width() -right - left, top), img,
                           QRectF(leftDIP, 0, size.width() - rightDIP - leftDIP, topDIP));
        if (left > 0) //top-left
            painter->drawImage(QRectF(rect.left(), rect.top(), left, top), img,
                               QRectF(0, 0, leftDIP, topDIP));
        if (right > 0) //top-right
            painter->drawImage(QRectF(rect.left() + rect.width() - right, rect.top(), right, top), img,
                               QRectF(size.width() - rightDIP, 0, rightDIP, topDIP));
    }
    //left
    if (left > 0)
        painter->drawImage(QRectF(rect.left(), rect.top()+top, left, rect.height() - top - bottom), img,
                           QRectF(0, topDIP, leftDIP, size.height() - bottomDIP - topDIP));
    //center
    painter->drawImage(QRectF(rect.left() + left, rect.top()+top, rect.width() -right - left,
                              rect.height() - bottom - top), img,
                       QRectF(leftDIP, topDIP, size.width() - rightDIP - leftDIP,
                              size.height() - bottomDIP - topDIP));
    if (right > 0) //right
        painter->drawImage(QRectF(rect.left() +rect.width() - right, rect.top()+top, right, rect.height() - top - bottom), img,
                           QRectF(size.width() - rightDIP, topDIP, rightDIP, size.height() - bottomDIP - topDIP));
    if (bottom > 0) { //bottom
        painter->drawImage(QRectF(rect.left() +left, rect.top() + rect.height() - bottom,
                                  rect.width() - right - left, bottom), img,
                           QRectF(leftDIP, size.height() - bottomDIP,
                                  size.width() - rightDIP - leftDIP, bottomDIP));
        if (left > 0) //bottom-left
            painter->drawImage(QRectF(rect.left(), rect.top() + rect.height() - bottom, left, bottom), img,
                               QRectF(0, size.height() - bottomDIP, leftDIP, bottomDIP));
        if (right > 0) //bottom-right
            painter->drawImage(QRectF(rect.left() + rect.width() - right, rect.top() + rect.height() - bottom, right, bottom), img,
                               QRectF(size.width() - rightDIP, size.height() - bottomDIP, rightDIP, bottomDIP));
    }
}

IntroductionWidget::IntroductionWidget(QWidget *parent)
    : QWidget(parent),
      m_borderImage(":/Resources/border.png")
{
    setFocusPolicy(Qt::StrongFocus);
    setFocus();
    parentWidget()->installEventFilter(this);

    QPalette p = palette();
    p.setColor(QPalette::WindowText, QColor(220, 220, 220));
    setPalette(p);

    m_textWidget = new QWidget(this);
    auto layout = new QVBoxLayout;
    m_textWidget->setLayout(layout);

    m_stepText = new QLabel(this);
    m_stepText->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stepText->setWordWrap(true);
    m_stepText->setTextFormat(Qt::RichText);
    m_stepText->setPalette(palette());
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
        btn->setPalette(palette());
        btn->setAutoDefault(false);
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

    m_bodyCss = "font-size: 16px;";
    m_items = {
        {QLatin1String("dwLabels"),
         tr("标签管理"),
         tr("通过树形结构来对标签进行展示和管理"),
         tr("<ul>"
            "<li>工具栏中的按钮用于管理标签，可以根据标签之间的关联来构建成树形结构，或者简单的列表形式。</li>"
            "<li>操作包括：新建标签，新建子标签，删除标签，上移标签，下移标签，左移标签，右移标签。</li>"
            "<li>建立好的标签结构会保存到 xml 文件，下次启动时自动读取展示。</li>"
            "<li>可以点击选中一个标签，也可以按住 Ctrl 或 Shift 来选中多个标签。</li>"
            "<li>为文件关联标签时，会用到当前选中的标签。</li>"
            "</ul>")},
        {QLatin1String("dwCurDirLabels"),
         tr("当前目录标签"),
         tr("展示当前浏览目录中文件的所有标签"),
         tr("<ul>"
            "<li>遍历文件后，该窗口会显示当前目录下所有文件使用的标签。</li>"
            "<li>点击标签可以快速将其选中，用于查找文件等操作。</li>"
            "<li>切换浏览目录时，标签列表会自动更新。</li>"
            "</ul>")},
        {QLatin1String("centralWidget"),
         tr("文件浏览"),
         tr("模拟 Windows 文件管理器，用于查看和管理文件"),
         tr("<ul>"
            "<li>分为两个部分：左侧的目录导航栏和右侧的文件显示区。</li>"
            "<li>支持两种视图模式：详细列表视图和缩略图视图，右键可切换。</li>"
            "<li>可以点击选中单个文件或文件夹，也可按住 Ctrl 或 Shift 选中多个。</li>"
            "<li>文件显示区选中文件，标签管理区选中标签，即可通过工具栏为文件添加标签。</li>"
            "<li>在缩略图模式下，双击文件标签区域可内联编辑标签。</li>"
            "<li>双击文件夹可进入该目录，双击文件会使用默认程序打开。</li>"
            "</ul>")},
        {QLatin1String("mainToolBar"),
         tr("工具栏"),
         tr("根据选中的文件和标签执行各种操作"),
         tr("<p style=\"margin-top: 20px\">各个按钮的说明如下：<table>"
             "<tr><td style=\"padding-right: 20px\">AI自动生成标签:</td><td>调用 AI 分析选中文件并自动生成标签。</td></tr>"
             "<tr><td style=\"padding-right: 20px\">添加标签:</td><td>根据标签区选中的标签为文件查看区选中的文件添加标签。</td></tr>"
             "<tr><td style=\"padding-right: 20px\">删除标签:</td><td>删除选中文件的所有标签。</td></tr>"
             "<tr><td style=\"padding-right: 20px\">查找文件:</td><td>根据选中的标签，在选中的文件夹中查找匹配的文件。</td></tr>"
             "<tr><td style=\"padding-right: 20px\">遍历文件:</td><td>遍历选中的文件夹，将标签信息存入缓存数据库以加速后续查询。</td></tr>"
             "<tr><td>界面介绍:</td><td>本界面介绍向导。</td></tr>"
             "</table></p>")},
        {QLatin1String("menuFileSearch"),
         tr("查找文件菜单"),
         tr("查找和遍历文件的相关菜单"),
         tr("<ul>"
             "<li>查找文件：根据标签区选中的标签查找匹配文件。</li>"
             "<li>遍历文件：遍历选中文件夹，将结果存入缓存数据库。</li>"
             "<li>设置查找条件：配置多个标签之间的逻辑关系（与/或），以及查找方式（从缓存数据库或直接从磁盘查询）。</li>"
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

const int SPOTLIGHTMARGIN = 18;
const int POINTER_WIDTH = 3;

void IntroductionWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setOpacity(.87);
    const QColor backgroundColor = Qt::black;
    if (m_stepPointerAnchor) {
        const QPoint anchorPos = m_stepPointerAnchor->mapTo(parentWidget(), QPoint{0, 0});
        const QRect anchorRect(anchorPos, m_stepPointerAnchor->size());
        const QRect spotlightRect = anchorRect.adjusted(-SPOTLIGHTMARGIN,
                                                        -SPOTLIGHTMARGIN,
                                                        SPOTLIGHTMARGIN,
                                                        SPOTLIGHTMARGIN);

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

        // smooth borders of the spotlighted area by gradients
        drawCornerImage(m_borderImage,
                         &p,
                         spotlightRect,
                         SPOTLIGHTMARGIN,
                         SPOTLIGHTMARGIN,
                         SPOTLIGHTMARGIN,
                         SPOTLIGHTMARGIN);

        // draw pointer
        const QColor qtGreen(65, 205, 82);
        p.setOpacity(1.);
        p.setPen(QPen(QBrush(qtGreen),
                      POINTER_WIDTH,
                      Qt::SolidLine,
                      Qt::RoundCap,
                      Qt::MiterJoin));
        p.setRenderHint(QPainter::Antialiasing);
        //指向工具栏和菜单栏时，箭头显示错误，去掉箭头
//        for (const QPolygonF &poly : pointerPolygon(spotlightRect, rect()))
//            p.drawPolyline(poly);
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
        // 特殊处理 QMenu：通过 actionGeometry 获取菜单在菜单栏中的位置
        if (anchorObjectName == "menuFileSearch") {
            auto *menuBar = parentWidget()->findChild<QMenuBar *>("menuBar");
            auto *menu = parentWidget()->findChild<QMenu *>(anchorObjectName);
            if (menuBar && menu) {
                QAction *action = menu->menuAction();
                if (action) {
                    // actionGeometry 返回的是相对菜单栏的坐标，转换为父窗口坐标
                    const QPoint menuBarPos = menuBar->mapTo(parentWidget(), QPoint{0, 0});
                    const QRect actionRect = menuBar->actionGeometry(action).translated(menuBarPos);
                    m_tempSpotlightWidget = new QWidget(parentWidget());
                    m_tempSpotlightWidget->setGeometry(actionRect);
                    m_tempSpotlightWidget->raise();
                    m_stepPointerAnchor = m_tempSpotlightWidget;
                }
            }
        } else {
            m_stepPointerAnchor = parentWidget()->findChild<QWidget *>(anchorObjectName);
        }
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
    } else {
        m_btnNext->setText(tr("下一步"));
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

// 在本工程中，文件查看区域较大，占据了界面的中心区域，所以使得显示在中心位置的说明文字显示不清晰
// 应该将说明文字显示在聚光灯锚点之外的空白区域
void IntroductionWidget::calTextGeometry()
{
    if(parentWidget() == nullptr)
    {
        return;
    }

    const int margin = 20;
    QRect textRect(width()/4, height()/4, width()/2, height()/2);

    if(m_stepPointerAnchor) {
        // 将锚点映射到父窗口坐标系，加上聚光灯边缘
        const QPoint anchorPos = m_stepPointerAnchor->mapTo(parentWidget(), QPoint{0, 0});
        const QRect anchorRect(anchorPos, m_stepPointerAnchor->size());
        const QRect spotlightRect = anchorRect.adjusted(-SPOTLIGHTMARGIN,
                                                        -SPOTLIGHTMARGIN,
                                                        SPOTLIGHTMARGIN,
                                                        SPOTLIGHTMARGIN);

        // 计算聚光灯四周的可用空间
        const int leftSpace  = spotlightRect.left() - margin;
        const int rightSpace = width() - spotlightRect.right() - margin;
        const int topSpace   = spotlightRect.top() - margin;
        const int botSpace   = height() - spotlightRect.bottom() - margin;

        // 找到空间最大的方向
        const int maxSpace = qMax(qMax(leftSpace, rightSpace), qMax(topSpace, botSpace));

        if (maxSpace <= 0) {
            // 聚光灯占据了整个窗口，回退到默认居中
            textRect = QRect(width()/4, height()/4, width()/2, height()/2);
        } else if (maxSpace == rightSpace && rightSpace >= width() * 0.4) {
            // 右侧空间足够，放在右边
            textRect = QRect(spotlightRect.right() + margin,
                             qMax(margin, (height() - height()/2) / 2),
                             rightSpace, height()/2);
        } else if (maxSpace == leftSpace && leftSpace >= width() * 0.4) {
            // 左侧空间足够，放在左边
            textRect = QRect(margin,
                             qMax(margin, (height() - height()/2) / 2),
                             leftSpace, height()/2);
        } else if (maxSpace == topSpace && topSpace >= height() * 0.3) {
            // 上方空间足够，放在上面
            textRect = QRect(qMax(margin, (width() - width()/2) / 2),
                             margin,
                             width()/2, topSpace);
        } else if (maxSpace == botSpace && botSpace >= height() * 0.3) {
            // 下方空间足够，放在下面
            textRect = QRect(qMax(margin, (width() - width()/2) / 2),
                             spotlightRect.bottom() + margin,
                             width()/2, botSpace);
        } else {
            // 没有足够的单独空间，回退到默认居中
            textRect = QRect(width()/4, height()/4, width()/2, height()/2);
        }
    }

    // 确保文本区域大小合理
    if (textRect.width() < 100)
        textRect.setWidth(100);
    if (textRect.height() < 100)
        textRect.setHeight(100);

    m_textWidget->setGeometry(textRect);
}
