#ifndef DUALLISTWIDGET_H
#define DUALLISTWIDGET_H

#include <QWidget>
#include <QStringList>

class QListWidget;
class QListWidgetItem;
class QLabel;
class QPushButton;

// Generic "shuttle list" control: two QListWidgets ("Available" / "Selected")
// with Add/Remove/Move Up/Move Down buttons between them. Deals purely in
// QStringList labels and QList<int> indices -- it has no knowledge of what
// those indices mean to any particular caller (an ini key, an enum, whatever).
// Callers own all of that themselves via setItemLists()/availableItems()/
// visibleItems()/itemsChanged(), so this widget can be reused for any other
// "pick and order a subset of N labeled things" configuration task without
// dragging along anything caller-specific.
class DualListWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QString availableLabel READ availableLabel WRITE setAvailableLabel)
    Q_PROPERTY(QString visibleLabel READ visibleLabel WRITE setVisibleLabel)

public:
    explicit DualListWidget(QWidget *parent = nullptr);

    // headerLabels[idx] is the display text for index idx. available/selected
    // are index lists -- every index appears in exactly one of the two lists,
    // never both, never neither. The first `frozen` entries of `selected` are
    // pinned at the top of "Selected": not selectable, removable, or
    // reorderable. That invariant is enforced structurally (frozen rows are
    // disabled, so nothing can ever move them), not by bounds-checking a
    // count, so there's no separate min/max-items concept to configure.
    void setItemLists(const QStringList &headerLabels,
                       const QList<int> &available,
                       const QList<int> &selected,
                       int frozen);
    QList<int> availableItems() const;
    QList<int> visibleItems() const;

    QString availableLabel() const;
    void setAvailableLabel(const QString &text);
    QString visibleLabel() const;
    void setVisibleLabel(const QString &text);

signals:
    // Fires after Add/Remove/Move Up/Move Down actually changes "Selected"'s
    // contents or order. Callers read visibleItems() (and availableItems(),
    // if they care) in response -- this widget doesn't know or care what they
    // do with them.
    void itemsChanged();

private slots:
    void addItems();
    void removeItems();
    void moveUp();
    void moveDown();

private:
    static void insertSortedByIndex(QListWidget *list, QListWidgetItem *item);

    QListWidget *m_listAvailable;
    QListWidget *m_listSelected;
    QPushButton *m_addButton;
    QPushButton *m_removeButton;
    QPushButton *m_moveUpButton;
    QPushButton *m_moveDownButton;
    QLabel *m_availableLabel;
    QLabel *m_visibleLabel;

    int m_frozen = 0;
};

#endif // DUALLISTWIDGET_H
