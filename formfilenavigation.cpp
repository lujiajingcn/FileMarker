#include "formfilenavigation.h"
#include "ui_formfilenavigation.h"

FormFileNavigation::FormFileNavigation(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::FormFileNavigation)
{
    ui->setupUi(this);

    m_tvNavigation = ui->tvNavigation;
    m_modelNavigation = new QFileSystemModel;
    m_tvNavigation->setModel(m_modelNavigation);

    m_modelNavigation->setRootPath("");

    // QFileSystemModel 默认有四列信息：Name、Size、Type、Date Modified，
    // 我们只需要Name，其余三列隐藏。
    m_tvNavigation->setColumnHidden(1, true);
    m_tvNavigation->setColumnHidden(2, true);
    m_tvNavigation->setColumnHidden(3, true);

    // 只显示文件夹
    m_modelNavigation->setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    // 文件夹根据名称升序排序
    m_tvNavigation->setRootIndex(m_modelNavigation->index(""));
    m_tvNavigation->header()->setSortIndicator(0,Qt::AscendingOrder);
    m_tvNavigation->setSortingEnabled(true);
    m_tvNavigation->sortByColumn(0, Qt::AscendingOrder);
}

FormFileNavigation::~FormFileNavigation()
{
    delete ui;
}

void FormFileNavigation::on_tvNavigation_clicked(const QModelIndex &index)
{
    emit sendNavigationClicked(index);
}
