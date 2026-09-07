#ifndef NOSCROLLWIDGETS_H
#define NOSCROLLWIDGETS_H

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSpinBox>

// Stock QSpinBox/QDoubleSpinBox/QComboBox/QSlider react to the mouse wheel
// regardless of focus, so scrolling a page of controls silently changes
// whichever one the cursor passes over. These require actual focus (click
// or Tab in) first; otherwise the event is ignored, which Qt automatically
// re-delivers to the parent so page/scroll-area scrolling isn't blocked.

class NoScrollSpinBox : public QSpinBox
{
public:
    using QSpinBox::QSpinBox;
protected:
    void wheelEvent(QWheelEvent *event) override;
};

class NoScrollDoubleSpinBox : public QDoubleSpinBox
{
public:
    using QDoubleSpinBox::QDoubleSpinBox;
protected:
    void wheelEvent(QWheelEvent *event) override;
};

class NoScrollComboBox : public QComboBox
{
public:
    using QComboBox::QComboBox;
protected:
    void wheelEvent(QWheelEvent *event) override;
};

class NoScrollSlider : public QSlider
{
public:
    using QSlider::QSlider;
protected:
    void wheelEvent(QWheelEvent *event) override;
};

#endif // NOSCROLLWIDGETS_H
