#ifndef INTRODUCTIONWIDGET_H
#define INTRODUCTIONWIDGET_H

#include <QEvent>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QHBoxLayout>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QPainter>
#include <QPointer>
#include <QPushButton>
#include <QVBoxLayout>

struct Item
{
    QString pointerAnchorObjectName;
    QString title;
    QString brief;
    QString description;
};

class IntroductionWidget : public QWidget
{
public:
    explicit IntroductionWidget(QWidget *parent = nullptr);
    ~IntroductionWidget() override;

protected:
    bool event(QEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void paintEvent(QPaintEvent *ev) override;
    void keyPressEvent(QKeyEvent *ke) override;
    void mouseReleaseEvent(QMouseEvent *me) override;

    void calTextGeometry();

private:
    void finish();
    void step();
    void setStep(uint index);
    void resizeToParent();
    void updateButtonStates();

    QWidget *m_textWidget;
    QLabel *m_stepText;
    QLabel *m_continueLabel;
    QHBoxLayout *m_buttonLayout;
    QPushButton *m_btnSkip;
    QPushButton *m_btnPrev;
    QPushButton *m_btnNext;
    QImage m_borderImage;
    QString m_bodyCss;
    std::vector<Item> m_items;
    QPointer<QWidget> m_stepPointerAnchor;
    QPointer<QWidget> m_tempSpotlightWidget;
    uint m_step = 0;
};
#endif //INTRODUCTIONWIDGET_H
