#ifndef TDRCHART_H
#define TDRCHART_H

#include <QList>
#include <QQuickPaintedItem>
#include <QtQml/qqmlregistration.h>

// One TDR trace (impulse, step or impedance) against distance, drawn with
// QPainter. Dense traces are drawn as a min/max envelope per pixel column
// so narrow reflections aren't lost.
class TdrChart : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QList<double> values READ values WRITE setValues)
    // Distance between consecutive values, in `unit`.
    Q_PROPERTY(double xStep READ xStep WRITE setXStep)
    Q_PROPERTY(QString unit READ unit WRITE setUnit)
    // Index marked by the cursor; -1 for none. Touch/drag emits pointSelected().
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex)

public:
    explicit TdrChart(QQuickItem* parent = nullptr);

    void paint(QPainter* painter) override;
    QList<double> values() const { return m_values; }
    void setValues(const QList<double>& v);
    double xStep() const { return m_xStep; }
    void setXStep(double v);
    QString unit() const { return m_unit; }
    void setUnit(const QString& v);
    int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int v);

signals:
    void pointSelected(int index);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseUngrabEvent() override;

private:
    QRectF plotRect() const;
    void selectAt(qreal x);
    QList<double> m_values;
    double m_xStep = 1;
    QString m_unit;
    int m_selectedIndex = -1;
};

#endif // TDRCHART_H
