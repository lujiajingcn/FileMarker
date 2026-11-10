#include "formfilenavigation.h"
#include "ui_formfilenavigation.h"

FormFileNavigation::FormFileNavigation(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormFileNavigation)
{
    ui->setupUi(this);

    m_navigationTree = ui->tvNavigation;
    m_modelNavigation = new QFileSystemModel(this);
    m_navigationTree->setModel(m_modelNavigation);

    // 显式连接目录树点击（此前依赖 on_<对象名>_<信号名> 的自动连接）
    connect(m_navigationTree, &QTreeView::clicked, this, &FormFileNavigation::onTvNavigationClicked);

    m_modelNavigation->setRootPath("");

    // QFileSystemModel 默认有四列信息：Name、Size、Type、Date Modified，
    // 我们只需要Name，其余三列隐藏。
    m_navigationTree->setColumnHidden(1, true);
    m_navigationTree->setColumnHidden(2, true);
    m_navigationTree->setColumnHidden(3, true);

    // 只显示文件夹
    m_modelNavigation->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    // 文件夹根据名称升序排序
    m_navigationTree->setRootIndex(m_modelNavigation->index(""));
    m_navigationTree->header()->setSortIndicator(0,Qt::AscendingOrder);
    m_navigationTree->setSortingEnabled(true);
    m_navigationTree->sortByColumn(0, Qt::AscendingOrder);

    // 隐藏表头
    m_navigationTree->header()->hide();
}

FormFileNavigation::~FormFileNavigation()
{
    delete ui;
}

void FormFileNavigation::onTvNavigationClicked(const QModelIndex &index)
{
    emit sendNavigationClicked(index);
}
