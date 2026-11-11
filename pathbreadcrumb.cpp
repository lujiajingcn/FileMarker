#include "pathbreadcrumb.h"

#include "common.h"
#include "formpagefilesutil.h"
#include "toolicons.h"

#include <QDir>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMenu>
#include <QResizeEvent>
#include <QToolButton>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

// 单个路径段最宽多少像素，超过就省略中间部分
const int kMaxSegmentWidth = 190;

QString makeStyleSheet()
{
    // 扁平样式，和资源管理器一致：白底、无边框、悬停变灰
    return QStringLiteral(
        "QToolButton { border: none; background: transparent; padding: 2px 4px;"
        "              color: #1a1a1a; }"
        "QToolButton:hover { background: #e8e8e8; border-radius: 3px; }"
        "QToolButton:pressed { background: #dcdcdc; border-radius: 3px; }"
        "QToolButton#bcSeparator { color: #8a8a8a; padding: 2px 0px; min-width: 10px; }"
        "QToolButton#bcSeparator:hover { background: transparent; }"
        "QLineEdit { border: 1px solid #c8c8c8; border-radius: 3px; padding: 2px 4px; }");
}

}   // namespace

PathBreadcrumb::PathBreadcrumb(QWidget *parent)
    : QWidget(parent),
      m_layout(new QHBoxLayout(this)),
      m_edit(new QLineEdit(this)),
      m_overflowButton(nullptr),
      m_menuButton(nullptr),
      m_editing(false)
{
    setObjectName(QStringLiteral("pathBreadcrumb"));
    m_layout->setContentsMargins(2, 0, 2, 0);
    m_layout->setSpacing(0);
    // 让面包屑能随窗口收缩：内容超宽时由 relayoutSegments() 折叠中段
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(80);

    m_edit->setVisible(false);
    m_edit->installEventFilter(this);
    connect(m_edit, &QLineEdit::returnPressed, this, [this]() { leaveEditMode(true); });

    setStyleSheet(makeStyleSheet());
    setContextMenuPolicy(Qt::DefaultContextMenu);
}

QString PathBreadcrumb::elided(const QString &text, int maxWidth, const QFont &font)
{
    const QFontMetrics metrics(font);
    if (metrics.horizontalAdvance(text) <= maxWidth)
        return text;
    return metrics.elidedText(text, Qt::ElideMiddle, maxWidth);
}

QString PathBreadcrumb::driveDisplayName(const QString &rootPath)
{
    // 盘符显示成"卷标 (E:)"，没有卷标就叫"本地磁盘 (E:)"，和资源管理器一致
    const QString sRoot = QDir::toNativeSeparators(rootPath).left(2);   // 只显示 "E:"，不带反斜杠
#ifdef Q_OS_WIN
    const std::wstring wRoot = QDir::toNativeSeparators(rootPath).toStdWString();
    wchar_t volumeName[MAX_PATH + 1] = {0};
    if (::GetVolumeInformationW(wRoot.c_str(), volumeName, MAX_PATH, nullptr, nullptr,
                                nullptr, nullptr, 0)) {
        const QString sVolume = QString::fromWCharArray(volumeName).trimmed();
        if (!sVolume.isEmpty())
            return QStringLiteral("%1 (%2)").arg(sVolume, sRoot);
    }
#else
    Q_UNUSED(sRoot);
#endif
    return QStringLiteral("%1 (%2)").arg(tr("本地磁盘"), sRoot);
}

void PathBreadcrumb::setPath(const QString &path)
{
    const QString sClean = path.isEmpty() ? QString() : QDir::cleanPath(path);
    if (sClean == m_path && !m_segmentButtons.isEmpty())
        return;
    m_path = sClean;
    if (m_editing)
        m_edit->setText(m_path);
    rebuild();
}

void PathBreadcrumb::rebuild()
{
    // 清掉旧的段与分隔符
    for (QToolButton *button : m_segmentButtons)
        button->deleteLater();
    for (QToolButton *button : m_separators)
        button->deleteLater();
    m_segmentButtons.clear();
    m_separators.clear();
    if (m_overflowButton != nullptr) {
        m_overflowButton->deleteLater();
        m_overflowButton = nullptr;
    }
    if (m_menuButton != nullptr) {
        m_menuButton->deleteLater();
        m_menuButton = nullptr;
    }

    // 组装新的段：我的电脑 › 盘符 › 目录1 › 目录2 ...
    struct Segment
    {
        QString title;
        QString path;
    };
    QList<Segment> segments;

    const bool bMyComputer = m_path.isEmpty()
                             || m_path.compare(FmPageUtil::myComputerName(), Qt::CaseInsensitive) == 0;
    segments.append({tr("我的电脑"), QString()});

    if (!bMyComputer) {
        // 盘符/根路径作为第二段
        QFileInfo info(m_path);
        const QString sRootPath = QDir::toNativeSeparators(info.absolutePath()).left(2) + QLatin1Char('/');
        QString sRootTitle;
        if (m_path.size() >= 2 && m_path.at(1) == QLatin1Char(':')) {
            sRootTitle = driveDisplayName(m_path.left(2) + QLatin1Char('\\'));
            segments.append({sRootTitle, m_path.left(2) + QLatin1Char('/')});
        } else {
            segments.append({QStringLiteral("/"), QStringLiteral("/")});
        }

        // 逐级追加：只追加到当前路径为止
        const QString sNative = QDir::toNativeSeparators(m_path);
        const int nSkip = segments.last().path.size();            // 已包含的根路径长度
        const QStringList qLParts = sNative.mid(nSkip).split(QLatin1Char('\\'), QString::SkipEmptyParts);
        QString sAccum = segments.last().path;
        for (const QString &sPart : qLParts) {
            if (!sAccum.endsWith(QLatin1Char('/')))
                sAccum += QLatin1Char('/');
            sAccum += sPart;
            segments.append({sPart, sAccum});
        }
    }

    int index = 0;
    for (const Segment &segment : segments) {
        if (index > 0) {
            auto *separator = new QToolButton(this);
            separator->setObjectName(QStringLiteral("bcSeparator"));
            separator->setText(QStringLiteral("›"));
            separator->setEnabled(false);   // 只当分隔符，不响应点击
            separator->setFocusPolicy(Qt::NoFocus);
            separator->setAutoRaise(true);
            m_layout->addWidget(separator);
            m_separators.append(separator);
        }

        auto *button = new QToolButton(this);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::NoFocus);   // 键盘焦点留给文件列表
        button->setToolTip(segment.path.isEmpty() ? tr("我的电脑") : segment.path);
        button->setText(elided(segment.title, kMaxSegmentWidth, font()));
        const QString sTarget = segment.path;
        if (index == 0) {
            // 第一段是"我的电脑"，前面加一个显示器图标（和资源管理器一致）
            button->setIcon(ToolIcons::make(ToolIcons::Computer));
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            button->setIconSize(QSize(16, 16));
        }
        connect(button, &QToolButton::clicked, this, [this, sTarget]() {
            if (m_editing)
                return;
            emit pathActivated(sTarget);
        });
        m_layout->addWidget(button);
        m_segmentButtons.append(button);
        ++index;
    }

    // 末尾留白 + 一个"最近访问"箭头按钮（沿用原来的历史下拉）
    m_menuButton = new QToolButton(this);
    m_menuButton->setObjectName(QStringLiteral("bcSeparator"));
    m_menuButton->setText(QStringLiteral("▾"));
    m_menuButton->setToolTip(tr("最近访问过的目录"));
    m_menuButton->setFocusPolicy(Qt::NoFocus);
    m_menuButton->setAutoRaise(true);
    connect(m_menuButton, &QToolButton::clicked, this, [this]() {
        emit recentDirsRequested(mapToGlobal(QPoint(0, height())));
    });
    m_layout->addWidget(m_menuButton);
    m_layout->addStretch(1);

    m_layout->addWidget(m_edit);
    relayoutSegments();
    if (m_editing) {
        m_edit->setVisible(true);
        m_edit->setText(m_path);
    }
}

void PathBreadcrumb::relayoutSegments()
{
    // 宽度不够时，把中间几段折进"…"按钮里（和资源管理器一致：首尾始终可见）
    if (m_segmentButtons.isEmpty())
        return;

    const int nAvailable = width() - 20;
    int nTotal = m_menuButton != nullptr ? m_menuButton->sizeHint().width() : 0;
    for (QToolButton *button : m_segmentButtons)
        nTotal += button->sizeHint().width();
    for (QToolButton *separator : m_separators)
        nTotal += separator->sizeHint().width();

    const bool bNeedOverflow = nTotal > nAvailable && m_segmentButtons.size() > 2;

    if (m_overflowButton != nullptr) {
        m_overflowButton->deleteLater();
        m_overflowButton = nullptr;
    }

    // 先把所有段恢复显示
    for (QToolButton *button : m_segmentButtons)
        button->setVisible(!m_editing);
    for (QToolButton *separator : m_separators) {
        separator->setVisible(!m_editing);
    }

    if (!bNeedOverflow)
        return;

    // 从中间开始折叠：保留第一段和最后一段
    int nUsed = m_segmentButtons.first()->sizeHint().width()
                + m_segmentButtons.last()->sizeHint().width();
    int nFirstHide = 1;
    int nLastHide = m_segmentButtons.size() - 2;
    int nHiddenCount = 0;
    for (int i = 1; i < m_segmentButtons.size() - 1; ++i) {
        const int nWidth = m_segmentButtons.at(i)->sizeHint().width();
        if (nUsed + nWidth > nAvailable)
            break;
        nUsed += nWidth;
        ++nFirstHide;
        --nHiddenCount;
    }
    Q_UNUSED(nLastHide);

    // 隐藏中段（索引 1 ~ m_segmentButtons.size()-2）
    for (int i = 1; i <= m_segmentButtons.size() - 2; ++i) {
        if (i >= nFirstHide) {
            m_segmentButtons.at(i)->setVisible(false);
            ++nHiddenCount;
            if (i - 1 < m_separators.size())
                m_separators.at(i - 1)->setVisible(false);
        }
    }
    if (nHiddenCount <= 0)
        return;

    // 插一个"…"按钮，菜单里列出被折叠的段
    m_overflowButton = new QToolButton(this);
    m_overflowButton->setText(QStringLiteral("…"));
    m_overflowButton->setAutoRaise(true);
    m_overflowButton->setFocusPolicy(Qt::NoFocus);
    auto *menu = new QMenu(m_overflowButton);
    for (int i = 1; i <= m_segmentButtons.size() - 2; ++i) {
        if (!m_segmentButtons.at(i)->isVisible()) {
            QAction *action = menu->addAction(m_segmentButtons.at(i)->text());
            const QString sTarget = m_segmentButtons.at(i)->toolTip();
            connect(action, &QAction::triggered, this, [this, sTarget]() {
                emit pathActivated(sTarget);
            });
        }
    }
    m_overflowButton->setMenu(menu);
    m_overflowButton->setPopupMode(QToolButton::InstantPopup);
    m_layout->insertWidget(2, m_overflowButton);   // 放在"我的电脑"之后
}

void PathBreadcrumb::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutSegments();
}

void PathBreadcrumb::startEditing()
{
    enterEditMode();
}

void PathBreadcrumb::enterEditMode()
{
    if (m_editing)
        return;
    m_editing = true;
    for (QToolButton *button : m_segmentButtons)
        button->setVisible(false);
    for (QToolButton *separator : m_separators)
        separator->setVisible(false);
    if (m_overflowButton != nullptr)
        m_overflowButton->setVisible(false);
    if (m_menuButton != nullptr)
        m_menuButton->setVisible(false);

    m_edit->setText(m_path);
    m_edit->setVisible(true);
    m_edit->setFocus(Qt::OtherFocusReason);
    m_edit->selectAll();
}

void PathBreadcrumb::leaveEditMode(bool commit)
{
    if (!m_editing)
        return;
    m_editing = false;
    const QString sText = m_edit->text().trimmed();
    m_edit->setVisible(false);
    rebuild();
    if (commit && !sText.isEmpty())
        emit editCommitted(sText);
}

void PathBreadcrumb::mouseDoubleClickEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    enterEditMode();
}

void PathBreadcrumb::contextMenuEvent(QContextMenuEvent *event)
{
    emit recentDirsRequested(event->globalPos());
    event->accept();
}

bool PathBreadcrumb::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_edit) {
        if (event->type() == QEvent::KeyPress) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_Escape) {
                leaveEditMode(false);
                return true;
            }
        } else if (event->type() == QEvent::FocusOut) {
            leaveEditMode(false);
        }
    }
    return QWidget::eventFilter(watched, event);
}
