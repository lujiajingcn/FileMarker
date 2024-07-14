#ifndef INTRODUCTIONWIDGET_H
#define INTRODUCTIONWIDGET_H

#include <QEvent>
#include <QGuiApplication>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPointer>
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

protected:
    bool event(QEvent *e) override;
    bool eventFilter(QObject *obj, QEvent *ev) override;
    void paintEvent(QPaintEvent *ev) override;
    void keyPressEvent(QKeyEvent *ke) override;
    void mouseReleaseEvent(QMouseEvent *me) override;

private:
    void finish();
    void step();
    void setStep(uint index);
    void resizeToParent();

    QWidget *m_textWidget;
    QLabel *m_stepText;
    QLabel *m_continueLabel;
    QImage m_borderImage;
    QString m_bodyCss;
    std::vector<Item> m_items;
    QPointer<QWidget> m_stepPointerAnchor;
    uint m_step = 0;
};
#endif //INTRODUCTIONWIDGET_H
