#ifndef POPUP_H
#define POPUP_H

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QPropertyAnimation>
#include <QTimer>
#include <QSettings>
//#include <settings.h>

class PopUp : public QWidget
{
    Q_OBJECT

    // Translucency property
    Q_PROPERTY(float popupOpacity READ getPopupOpacity WRITE setPopupOpacity)

    void setPopupOpacity(float opacity);
    float getPopupOpacity() const;

public:
    explicit PopUp(QWidget *parent = 0);
    explicit PopUp(QString button, QWidget *parent = 0);
    ~PopUp();
    void init();
    void setName(QString name);
    int getDurability (void) const {return m_durability;}
    void setDurability (int durability) {m_durability = durability;}
    bool getHiding (void) const {return m_hiding;}
    void setHiding (bool hiding) {m_hiding = hiding;}

    void setX(int x){m_x = x;}
    void setY(int y){m_y = y;}
    void setPosition(int x, int y);
    void setParentPosition(int x, int y)
    {
        m_parentX = x;
        m_parentY = y;
    }

    void setBackgroundColor(QColor color){ m_bgColor = color;}
    void setPenColor(QColor color){ m_penColor = color;}
    void setTextColor(QString color){
        m_textColor = color;
        label.setStyleSheet("QLabel { color : " + m_textColor + ";"
                            "margin-top: 6px;"
                            "margin-bottom: 6px;"
                            "margin-left: 10px;"
                            "margin-right: 10px; }");}

    void MainWindowPos(int x, int y);

protected:
    void paintEvent(QPaintEvent *event);    // Background will be drawn via the repaint method
    void mousePressEvent(QMouseEvent * event);
    void mouseMoveEvent(QMouseEvent *);

public slots:
    void setPopupText(const QString& text); // Set the notification text
    QString getPopupText();
    void show();                            /* Own method for showing the widget
                                             * Needed for preliminary animation setup
                                             * */
    void hide();                            /* When the animation finishes, this slot checks
                                             * whether the widget is visible, or needs to be hidden
                                             * */
    void focusShow();
    void focusHide();

protected slots:
    void hideAnimation();                   // Slot to start the hide animation

signals:
    void canceled();

protected:
    QColor m_bgColor;
    QColor m_penColor;
    QString m_textColor;

protected:
    QLabel label;           // Label with the message
    QPushButton button;
    bool m_showButton = false;
    QString m_buttonName;
//    QPushButton button;     // Collapse button
//    QGridLayout layout;     // Layout for the label
    QVBoxLayout layout;
    QPropertyAnimation animation;   // Animation property for the popup message
    float m_popupOpacity;     // Widget's translucency property
    QTimer *timer;          // Timer after which the widget will be hidden
    int m_durability;
    bool m_hiding;
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


    QString m_name;

    QSettings *m_settings;

};


#endif // POPUP_H
