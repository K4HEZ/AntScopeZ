#ifndef CABLEPICKERDIALOG_H
#define CABLEPICKERDIALOG_H

#include <QDialog>
#include <QStringList>

class QLineEdit;
class QListWidget;
class QListWidgetItem;

// Searchable cable list: Recent (AppConfig::recentCables) above All cables.
class CablePickerDialog : public QDialog
{
    Q_OBJECT
public:
    // The picked name (also pushed onto the recents), or empty if cancelled.
    static QString pick(QWidget* parent, const QStringList& names, const QString& current);

private:
    CablePickerDialog(QWidget* parent, const QStringList& names, const QString& current);
    void rebuild();
    void choose(QListWidgetItem* item);

    QStringList m_names;
    QString m_current;
    QString m_picked;
    QLineEdit* m_search;
    QListWidget* m_list;
};

#endif // CABLEPICKERDIALOG_H
