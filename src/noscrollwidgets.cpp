#include "noscrollwidgets.h"
#include <QWheelEvent>

void NoScrollSpinBox::wheelEvent(QWheelEvent *event)
{
    if (hasFocus())
        QSpinBox::wheelEvent(event);
    else
        event->ignore();
}

void NoScrollDoubleSpinBox::wheelEvent(QWheelEvent *event)
{
    if (hasFocus())
        QDoubleSpinBox::wheelEvent(event);
    else
        event->ignore();
}

void NoScrollComboBox::wheelEvent(QWheelEvent *event)
{
    if (hasFocus())
        QComboBox::wheelEvent(event);
    else
        event->ignore();
}

void NoScrollSlider::wheelEvent(QWheelEvent *event)
{
    if (hasFocus())
        QSlider::wheelEvent(event);
    else
        event->ignore();
}
