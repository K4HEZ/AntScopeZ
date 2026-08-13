#include "duallistwidget.h"
#include "style.h"

#include <QGridLayout>
#include <QVBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>

#include <algorithm>

DualListWidget::DualListWidget(QWidget *parent)
    : QWidget(parent)
    , m_listAvailable(new QListWidget(this))
    , m_listSelected(new QListWidget(this))
    , m_addButton(new QPushButton(tr("Add >"), this))
    , m_removeButton(new QPushButton(tr("< Remove"), this))
    , m_moveUpButton(new QPushButton(tr("^"), this))
    , m_moveDownButton(new QPushButton(tr("v"), this))
    , m_availableLabel(new QLabel(tr("Available"), this))
    , m_visibleLabel(new QLabel(tr("Selected"), this))
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_listAvailable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_listSelected->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_listAvailable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listSelected->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_listAvailable->setStyleSheet(Style::listWidget());
    m_listSelected->setStyleSheet(Style::listWidget());

    for (QPushButton *button : {m_addButton, m_removeButton, m_moveUpButton, m_moveDownButton})
        button->setStyleSheet(Style::pushButton());

    for (QLabel *label : {m_availableLabel, m_visibleLabel})
        label->setStyleSheet(Style::label());

    // Add/Remove and Move Up/Move Down are two distinct action groups sharing
    // one column between the lists, stacked with a small gap between them and
    // centered vertically as a whole -- saves the horizontal real estate a
    // separate third column would cost.
    auto *buttonColumn = new QVBoxLayout;
    buttonColumn->addStretch();
    buttonColumn->addWidget(m_addButton);
    buttonColumn->addWidget(m_removeButton);
    buttonColumn->addSpacing(20);
    buttonColumn->addWidget(m_moveUpButton);
    buttonColumn->addWidget(m_moveDownButton);
    buttonColumn->addStretch();

    auto *grid = new QGridLayout(this);
    grid->addWidget(m_availableLabel, 0, 0);
    grid->addWidget(m_visibleLabel, 0, 2);
    grid->addWidget(m_listAvailable, 1, 0);
    grid->addLayout(buttonColumn, 1, 1);
    grid->addWidget(m_listSelected, 1, 2);

    connect(m_addButton, &QPushButton::clicked, this, &DualListWidget::addItems);
    connect(m_removeButton, &QPushButton::clicked, this, &DualListWidget::removeItems);
    connect(m_moveUpButton, &QPushButton::clicked, this, &DualListWidget::moveUp);
    connect(m_moveDownButton, &QPushButton::clicked, this, &DualListWidget::moveDown);
}

void DualListWidget::addItems()
{
    auto items = m_listAvailable->selectedItems();
    if (items.isEmpty())
        return;

    for (QListWidgetItem *item : items) {
        m_listAvailable->takeItem(m_listAvailable->row(item));
        m_listSelected->addItem(item);
    }

    emit itemsChanged();
}

void DualListWidget::removeItems()
{
    auto items = m_listSelected->selectedItems();
    if (items.isEmpty())
        return;

    for (QListWidgetItem *item : items) {
        m_listSelected->takeItem(m_listSelected->row(item));
        insertSortedByIndex(m_listAvailable, item);
    }

    emit itemsChanged();
}

// "Available" always stays in index order, regardless of where in that order
// an item gets removed back to.
void DualListWidget::insertSortedByIndex(QListWidget *list, QListWidgetItem *item)
{
    const int idx = item->data(Qt::UserRole).toInt();

    int row = 0;
    while (row < list->count() && list->item(row)->data(Qt::UserRole).toInt() < idx)
        ++row;

    list->insertItem(row, item);
}

void DualListWidget::moveUp()
{
    QListWidget *list = m_listSelected;
    QList<QListWidgetItem *> selected = list->selectedItems();

    if (selected.isEmpty())
        return;

    // selectedItems() isn't guaranteed to be in row order; the shift below
    // needs top-to-bottom order to be correct for multi-item selections.
    std::sort(selected.begin(), selected.end(), [list](QListWidgetItem *a, QListWidgetItem *b) {
        return list->row(a) < list->row(b);
    });

    // Find the highest selected item.
    int firstRow = list->row(selected.first());

    // If any selected item is at the top (or the frozen block below it),
    // don't move anything.
    if (firstRow <= m_frozen)
        return;

    // Move selected items from top to bottom.
    for (QListWidgetItem *item : selected) {
        int row = list->row(item);

        list->takeItem(row);
        list->insertItem(row - 1, item);
    }

    // Restore selection.
    for (QListWidgetItem *item : selected)
        item->setSelected(true);

    emit itemsChanged();
}

void DualListWidget::moveDown()
{
    QListWidget *list = m_listSelected;
    QList<QListWidgetItem *> selected = list->selectedItems();

    if (selected.isEmpty())
        return;

    // selectedItems() isn't guaranteed to be in row order; the shift below
    // needs bottom-to-top order to be correct for multi-item selections.
    std::sort(selected.begin(), selected.end(), [list](QListWidgetItem *a, QListWidgetItem *b) {
        return list->row(a) < list->row(b);
    });

    // Find the lowest selected item.
    int lastRow = list->row(selected.last());

    // If any selected item is at the bottom, don't move anything.
    if (lastRow == list->count() - 1)
        return;

    // Move selected items from bottom to top.
    for (int i = selected.size() - 1; i >= 0; --i) {
        QListWidgetItem *item = selected.at(i);

        int row = list->row(item);

        list->takeItem(row);
        list->insertItem(row + 1, item);
    }

    // Restore selection.
    for (QListWidgetItem *item : selected)
        item->setSelected(true);

    emit itemsChanged();
}

void DualListWidget::setItemLists(const QStringList &headerLabels,
                                   const QList<int> &available,
                                   const QList<int> &selected,
                                   int frozen)
{
    m_frozen = qBound(0, frozen, selected.size());

    m_listAvailable->clear();
    m_listSelected->clear();

    // Each item's original index rides along as Qt::UserRole data --
    // takeItem()/addItem() move the same QListWidgetItem* between the two
    // lists (add/remove/move-up/move-down), so it survives every operation
    // without needing to be looked up again by label text.
    for (int idx : available) {
        auto *item = new QListWidgetItem(headerLabels.value(idx));
        item->setData(Qt::UserRole, idx);
        m_listAvailable->addItem(item);
    }

    for (int idx : selected) {
        auto *item = new QListWidgetItem(headerLabels.value(idx));
        item->setData(Qt::UserRole, idx);
        m_listSelected->addItem(item);
    }

    // Frozen rows sit pinned at the top of "Selected" -- the user can't
    // select, remove, or reorder them, so grey them out and disable
    // interaction entirely rather than special-casing them in every slot.
    for (int i = 0; i < m_frozen; ++i) {
        QListWidgetItem *item = m_listSelected->item(i);
        item->setFlags(item->flags() & ~(Qt::ItemIsSelectable | Qt::ItemIsEnabled));
        item->setToolTip(tr("Always shown"));
    }
}

QList<int> DualListWidget::availableItems() const
{
    QList<int> result;

    for (int i = 0; i < m_listAvailable->count(); ++i)
        result << m_listAvailable->item(i)->data(Qt::UserRole).toInt();

    return result;
}

QList<int> DualListWidget::visibleItems() const
{
    QList<int> result;

    for (int i = 0; i < m_listSelected->count(); ++i)
        result << m_listSelected->item(i)->data(Qt::UserRole).toInt();

    return result;
}

QString DualListWidget::availableLabel() const
{
    return m_availableLabel->text();
}

void DualListWidget::setAvailableLabel(const QString &text)
{
    m_availableLabel->setText(text);
}

QString DualListWidget::visibleLabel() const
{
    return m_visibleLabel->text();
}

void DualListWidget::setVisibleLabel(const QString &text)
{
    m_visibleLabel->setText(text);
}
