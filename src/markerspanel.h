#ifndef MARKERSPANEL_H
#define MARKERSPANEL_H

#include <QObject>
#include <QWidget>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QSettings>
#include <settings.h>

struct MarkersHeaderColumn
{
    enum {
        fieldDelete, fieldNum, fieldSerie, fieldFQ, // fixed: 0-3
        fieldSWR, fieldRL, fieldPhase, fieldR, fieldX, fieldZ, // default: 4-9
        fieldL, fieldC, fieldRho, fieldZmod, // optional
        fieldRpar, fieldXpar, fieldZpar, fieldLpar, fieldCpar,
        // S21/S12 (2-port, from a real .s2p import -- see dataSParam) --
        // appended at the end, not interleaved, so existing saved column
        // selections (persisted by this enum's int values) don't shift.
        fieldS21, fieldS21Phase, fieldS12, fieldS12Phase,
    };

    // index/button are unused by MarkersPanel itself (see m_columnTypes
    // below instead) -- kept because PrintMarkers (printmarkers.h) still
    // uses this same struct for its own, independent QGridLayout-of-QLabels
    // print output.
    int index = -1;
    QWidget* button = nullptr;
    static QMap<int, QString>& headerMap();
    static QMap<int, QString> m_mapHeader;
};

// Docked, plain child widget -- lives in mainwindow.ui's right-pane
// splitter, under the plot tabs. Was MarkersPopUp, a floating Qt::Tool
// window with its own translucent painted background and chart-background-
// derived contrast colors (frameless, WA_TranslucentBackground, opacity
// animation, mouse-drag repositioning, persisted x/y) -- see git history if
// any of that ever needs resurrecting. Now a plain QTableWidget: native
// Fusion/palette rendering (Style::tableWidget()/headerView() are
// deliberately left empty -- see style.cpp) tracks the app's Light/Dark/etc.
// theme for free, and QTableWidget's own viewport gives horizontal/vertical
// scrollbars only as needed with no extra code.
class MarkersPanel : public QWidget
{
    Q_OBJECT

public:
    explicit MarkersPanel(QWidget *parent = nullptr);
    ~MarkersPanel();

public slots:
    void clearTable(void);
    void on_remove();
    void on_customContextMenuRequested(const QPoint& pos);
    QList<int> getColumns();
    void updateMarkers(int markers, int measurements, bool force = false);
    void updateInfo(QList<QList<QVariant>>& info);

    // Rebuilds the header/table from the current [Markers]header ini value.
    // Column choice/order is owned entirely by Settings' Markers tab
    // (DualListWidget) now -- this is a pure "reload and repaint yourself"
    // refresh, called after Settings has already written the new value, not
    // a place that writes ini itself.
    void reloadColumns();

    // Rebuilds header text in the newly-selected UI language.
    void on_translate();

protected:
    void createHeader();
    QString formatText(int type, QVariant val);

signals:
    void removeMarker(int);
    void changeColumns();
    // Issue #36 -- table's right-click "Clear All".
    void clearAllMarkers();

private:
    QVBoxLayout* m_layout;
    QTableWidget* m_table;

    int m_markers = 0;
    int m_measurements = 0;
    // Field type (MarkersHeaderColumn enum) per table column, in display
    // order -- column 0 is always fieldDelete (frozen, see Settings::
    // initMarkersTab()'s "frozen" boundary), the rest come straight from
    // the ini "header" value.
    QList<int> m_columnTypes;

    QSettings *m_settings;
};

#endif // MARKERSPANEL_H
