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
//
// QAbstractSpinBox defaults to Qt::WheelFocus, which grants focus as *part
// of* delivering the wheel event -- so checking hasFocus() in wheelEvent()
// alone doesn't work, it's always already true by then. Explicitly forcing
// Qt::StrongFocus (Tab + click, no wheel) in each constructor closes that.

class NoScrollSpinBox : public QSpinBox
{
public:
    explicit NoScrollSpinBox(QWidget *parent = nullptr);
protected:
    void wheelEvent(QWheelEvent *event) override;
};

class NoScrollDoubleSpinBox : public QDoubleSpinBox
{
public:
    explicit NoScrollDoubleSpinBox(QWidget *parent = nullptr);
protected:
    void wheelEvent(QWheelEvent *event) override;
};

class NoScrollComboBox : public QComboBox
{
public:
    explicit NoScrollComboBox(QWidget *parent = nullptr);
protected:
    void wheelEvent(QWheelEvent *event) override;
};

class NoScrollSlider : public QSlider
{
public:
    explicit NoScrollSlider(QWidget *parent = nullptr);
protected:
    void wheelEvent(QWheelEvent *event) override;
};

#endif // NOSCROLLWIDGETS_H
