#ifndef PRINTMARKERS_H
#define PRINTMARKERS_H

#include <QWidget>
#include <QVariant>
#include "markers.h"

class PrintMarkers : public QWidget
{
    Q_OBJECT

public:
    explicit PrintMarkers(QWidget *parent = 0);
    ~PrintMarkers();

protected:
    virtual void createHeader();
    virtual void initLayout();
    QString formatText(int type, QVariant val);
    void updateTable();

public slots:
    virtual void updateMarkers(int markers, int measurements);
    virtual void updateInfo(QList<QList<QVariant>>& info);
    // Plain text table instead of marker fields (TDR events).
    void setTable(const QStringList& headers, const QList<QStringList>& rows);
    // The table as text, whichever way it was filled, for the printout.
    void tableText(QStringList& headers, QList<QStringList>& rows) const;
    void clearTable(void);
    QList<int> getColumns();

signals:
    void changeColumns();

protected:
    int m_markers=0;
    int m_measurements=0;
    bool m_plainTable = false; // set by setTable()
    QStringList m_plainHeaders;
    QList<QStringList> m_plainRows;
    bool m_menuVisible = false;
    QList<MarkersHeaderColumn> m_headerColumns;
    QList<QList<QWidget*>> m_rows;
    QGridLayout m_layout;
    QSettings *m_settings;
};

#endif // PRINTMARKERS_H

