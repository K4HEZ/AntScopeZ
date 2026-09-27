#include "noscrollwidgets.h"
#include <QWheelEvent>

NoScrollSpinBox::NoScrollSpinBox(QWidget *parent) : QSpinBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void NoScrollSpinBox::wheelEvent(QWheelEvent *event)
{
    if (hasFocus())
        QSpinBox::wheelEvent(event);
    else
        event->ignore();
}

NoScrollDoubleSpinBox::NoScrollDoubleSpinBox(QWidget *parent) : QDoubleSpinBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void NoScrollDoubleSpinBox::wheelEvent(QWheelEvent *event)
{
    if (hasFocus())
        QDoubleSpinBox::wheelEvent(event);
    else
        event->ignore();
}

NoScrollComboBox::NoScrollComboBox(QWidget *parent) : QComboBox(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void NoScrollComboBox::wheelEvent(QWheelEvent *event)
{
    if (hasFocus())
        QComboBox::wheelEvent(event);
    else
        event->ignore();
}

NoScrollSlider::NoScrollSlider(QWidget *parent) : QSlider(parent)
{
    setFocusPolicy(Qt::StrongFocus);
}

void NoScrollSlider::wheelEvent(QWheelEvent *event)
{
    if (hasFocus())
        QSlider::wheelEvent(event);
    else
        event->ignore();
}
