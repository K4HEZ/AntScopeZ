#ifndef ONEFQWIDGET_H
#define ONEFQWIDGET_H

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include "analyzerparameters.h"
#include "settings.h"

// The two One-Fq floating-window styles. Detailed is this widget (11
// packed technical fields, see updateText()) -- a debug/inspection use
// case. BigReadout (OneFqBigReadout, onefqbigreadout.h) is a separate,
// resizable dialog serving the tuning-aid use case instead: a single
// large, glanceable SWR number, readable from across a yard. Measurements
// owns which style is currently live and swaps between them on
// double-click (see OneFqWidget::styleToggleRequested()); the choice is
// ini-persisted (sticky across sessions).
enum class OneFqDisplayStyle {
    Detailed,
    BigReadout,
};

class OneFqWidget : public QWidget
{
    Q_OBJECT

    QSettings *m_settings;
    QLabel m_label;
    QColor m_bgColor;
    QColor m_penColor;
    QColor m_textColor;
    int m_points;
    bool m_hintEnabled = true;
    bool m_shortHintEnabled = true;
    GraphData m_data;
    int m_added=0;
    int m_x;
    int m_y;
    int m_biasX;
    int m_biasY;
    int m_mainX;
    int m_mainY;
    int m_mainBiasX;
    int m_mainBiasY;
    int m_parentX;
    int m_parentY;

public:
    explicit OneFqWidget(int _points, QWidget *parent = 0);
    ~OneFqWidget();

    void setBackgroundColor(QColor color){ m_bgColor = color;}
    void setPenColor(QColor color){ m_penColor = color;}
    void setTextColor(QString _name)
    { 
        m_textColor = QColor::fromString(_name);
        m_label.setStyleSheet("QLabel { color : " + _name + ";"
                            "margin-top: 6px;"
                            "margin-bottom: 6px;"
                            "margin-left: 10px;"
                            "margin-right: 10px; }");}
    void addData(GraphData _data);
    GraphData& getData() { return m_data; }
    void reset() { m_added=0;  }
    int points() { return m_points; }
    bool needUpdate() { return (m_added >= m_points); }
    void MainWindowPos(int x, int y);
    void setX(int x){m_x = x;}
    void setY(int y){m_y = y;}
    void setPosition(int x, int y);
    void setParentPosition(int x, int y)
    {
        m_parentX = x;
        m_parentY = y;
    }

protected:
    void paintEvent(QPaintEvent *event);
    void mousePressEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent *);
    void mouseDoubleClickEvent(QMouseEvent *);
    void addValue(double src, double& dst);
    void updateText();

signals:
    void canceled(bool);
    // Double-click anywhere on the widget -- Measurements swaps this
    // Detailed style out for OneFqBigReadout (and back again on the
    // BigReadout's own double-click), see
    // Measurements::toggleOneFqDisplayStyle().
    void styleToggleRequested();

public slots:
    void setText(const QString& text);
    QString getText();
};

#endif // ONEFQWIDGET_H
