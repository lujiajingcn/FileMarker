// Copyright (C) 2018 The Qt Company Ltd.
// SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0

#include "introductionwidget.h"

#include <QEvent>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPointer>
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
    // why is palette not inherited???
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
    m_bodyCss = "font-size: 16px;";
    m_items = {
        {QLatin1String("dwLabels"),
         tr("标签管理"),
         tr("通过树形结构来对标签进行展示和管理"),
         tr("<ul>"
            "<li>工具栏中的按钮用于管理标签，可以根据标签之间的关联来构建成树形结构，或者简单的列表形式。</li>"
            "<li>操作包括：新建标签，新建子标签，删除标签，上移标签，下移标签，左移标签，右移标签</li>"
            "<li>建立好的标签结构会保持到xml文件，下次启动时自动读取展示。</li>"
            "<li>可以点击选中一个标签，也可以按住Ctrl或者Shift来选中多个标签。</li>"
            "<li>为文件关联标签时，会用到当前选中的标签。</li>"
            "</ul>")},
        {QLatin1String("centralWidget"),
         tr("文件浏览"),
         tr("模拟Windows的文件管理器，用于查看文件位置。"),
         tr("<ul>"
            "<li>分为两个部分，左侧的导航栏，右侧的文件显示区。</li>"
            "<li>可以点击选中一个文件或者文件夹，也可以按住Ctrl或者Shift来选中多个文件或文件夹。</li>"
            "<li>文件浏览区选中文件，标签管理区选中标签，就可以通过工具栏的关联按钮来为文件添加标签。</li>"
            "<li>对于已经有标签的文件，再次添加标签会自动删除掉之前的标签。</li>"
            "<li>选中文件右键，可以删除该文件的标签。</li>"
            "</ul>")},
        {QLatin1String("mainToolBar"),
         tr("工具栏"),
         tr(""),
         {}},
        {QLatin1String("menuBar"),
         tr("菜单"),
         tr(""),
         {}},
        {QLatin1String("statusBar"), tr("状态栏"), tr(""), {}},
        {{},
         tr("结束"),
         tr(" "
                "源码：<a style=\"color: #41CD52\" "
                "href=\"https://github.com/lujiajingcn/FileMarker\">FileMarker</a>."),
         {}}};
    setStep(0);
    resizeToParent();
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
const int POINTER_SIZE = 30;
const int POINTER_WIDTH = 3;

static int margin(const QRect &small, const QRect &big, Qt::Alignment side)
{
    switch (side) {
    case Qt::AlignRight:
        return qMax(0, big.right() - small.right());
    case Qt::AlignTop:
        return qMax(0, small.top() - big.top());
    case Qt::AlignBottom:
        return qMax(0, big.bottom() - small.bottom());
    case Qt::AlignLeft:
        return qMax(0, small.x() - big.x());
    default:
        return 0;
    }
}

static int oppositeMargin(const QRect &small, const QRect &big, Qt::Alignment side)
{
    switch (side) {
    case Qt::AlignRight:
        return margin(small, big, Qt::AlignLeft);
    case Qt::AlignTop:
        return margin(small, big, Qt::AlignBottom);
    case Qt::AlignBottom:
        return margin(small, big, Qt::AlignTop);
    case Qt::AlignLeft:
        return margin(small, big, Qt::AlignRight);
    default:
        return 100000;
    }
}

static const QVector<QPolygonF> pointerPolygon(const QRect &anchorRect, const QRect &fullRect)
{
    // Put the arrow opposite to the smallest margin,
    // with priority right, top, bottom, left.
    // Not very sophisticated but sufficient for current use cases.
    QVector<Qt::Alignment> alignments{Qt::AlignRight, Qt::AlignTop, Qt::AlignBottom, Qt::AlignLeft};
//    Utils::sort(alignments, [anchorRect, fullRect](Qt::Alignment a, Qt::Alignment b) {
//        return oppositeMargin(anchorRect, fullRect, a) < oppositeMargin(anchorRect, fullRect, b);
//    });
    const auto alignIt = std::find_if(alignments.cbegin(),
                                      alignments.cend(),
                                      [anchorRect, fullRect](Qt::Alignment align) {
                                          return margin(anchorRect, fullRect, align) > POINTER_SIZE;
                                      });
    if (alignIt == alignments.cend())
        return {{}}; // no side with sufficient space found
    const qreal arrowHeadWidth = POINTER_SIZE/3.;
    if (*alignIt == Qt::AlignRight) {
        const qreal middleY = anchorRect.center().y();
        const qreal startX = anchorRect.right() + POINTER_SIZE;
        const qreal endX = anchorRect.right() + POINTER_WIDTH;
        return {{QVector<QPointF>{{startX, middleY}, {endX, middleY}}},
                QVector<QPointF>{{endX + arrowHeadWidth, middleY - arrowHeadWidth},
                                 {endX, middleY},
                                 {endX + arrowHeadWidth, middleY + arrowHeadWidth}}};
    }
    if (*alignIt == Qt::AlignTop) {
        const qreal middleX = anchorRect.center().x();
        const qreal startY = anchorRect.y() - POINTER_SIZE;
        const qreal endY = anchorRect.y() - POINTER_WIDTH;
        return {{QVector<QPointF>{{middleX, startY}, {middleX, endY}}},
                QVector<QPointF>{{middleX - arrowHeadWidth, endY - arrowHeadWidth},
                                 {middleX, endY},
                                 {middleX + arrowHeadWidth, endY - arrowHeadWidth}}};
    }
    if (*alignIt == Qt::AlignBottom) {
        const qreal middleX = anchorRect.center().x();
        const qreal startY = anchorRect.y() + POINTER_WIDTH;
        const qreal endY = anchorRect.y() + POINTER_SIZE;
        return {{QVector<QPointF>{{middleX, startY}, {middleX, endY}}},
                QVector<QPointF>{{middleX - arrowHeadWidth, endY + arrowHeadWidth},
                                 {middleX, endY},
                                 {middleX + arrowHeadWidth, endY + arrowHeadWidth}}};
    }

    // Qt::AlignLeft
    const qreal middleY = anchorRect.center().y();
    const qreal startX = anchorRect.x() - POINTER_WIDTH;
    const qreal endX = anchorRect.x() - POINTER_SIZE;
    return {{QVector<QPointF>{{startX, middleY}, {endX, middleY}}},
            QVector<QPointF>{{endX - arrowHeadWidth, middleY - arrowHeadWidth},
                             {endX, middleY},
                             {endX - arrowHeadWidth, middleY + arrowHeadWidth}}};
}

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
        for (const QPolygonF &poly : pointerPolygon(spotlightRect, rect()))
            p.drawPolyline(poly);
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
    m_continueLabel->setText(tr("界面介绍 %1/%2 >").arg(m_step + 1).arg(m_items.size()));
    const Item &item = m_items.at(m_step);
    m_stepText->setText("<html><body style=\"" + m_bodyCss + "\">" + "<h1>" + item.title
                        + "</h1><p>" + item.brief + "</p>" + item.description + "</body></html>");
    const QString anchorObjectName = m_items.at(m_step).pointerAnchorObjectName;
    if (!anchorObjectName.isEmpty()) {
        m_stepPointerAnchor = parentWidget()->findChild<QWidget *>(anchorObjectName);
        if(m_stepPointerAnchor == nullptr)
        {
            return;
        }
    } else {
        m_stepPointerAnchor.clear();
    }
    update();
}

void IntroductionWidget::resizeToParent()
{
    if(parentWidget() == nullptr)
    {
        return;
    }
    setGeometry(QRect(QPoint(0, 0), parentWidget()->size()));
    m_textWidget->setGeometry(QRect(width()/4, height()/4, width()/2, height()/2));
}

