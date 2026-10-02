#include "cablepickerdialog.h"
#include "appconfig.h"
#include "cablecatalog.h"
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

CablePickerDialog::CablePickerDialog(QWidget* parent, const QStringList& names, const QString& current)
    : QDialog(parent), m_names(names), m_current(current)
{
    setWindowTitle(tr("Select cable preset"));
    resize(420, 520);

    m_search = new QLineEdit;
    m_search->setPlaceholderText(tr("Search cables"));
    m_search->setClearButtonEnabled(true);
    m_list = new QListWidget;

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(m_search);
    layout->addWidget(m_list);

    connect(m_search, &QLineEdit::textChanged, this, &CablePickerDialog::rebuild);
    connect(m_list, &QListWidget::itemActivated, this, &CablePickerDialog::choose);
    connect(m_list, &QListWidget::itemClicked, this, &CablePickerDialog::choose);
    // Enter in the search box picks the top match.
    connect(m_search, &QLineEdit::returnPressed, this, [this] {
        for (int i = 0; i < m_list->count(); ++i)
            if (m_list->item(i)->flags() & Qt::ItemIsSelectable) {
                choose(m_list->item(i));
                return;
            }
    });
    rebuild();
}

void CablePickerDialog::rebuild()
{
    m_list->clear();
    const bool filtering = !m_search->text().trimmed().isEmpty();

    auto addHeader = [this](const QString& text) {
        auto* h = new QListWidgetItem(text);
        QFont f = h->font();
        f.setBold(true);
        h->setFont(f);
        h->setFlags(Qt::NoItemFlags);
        m_list->addItem(h);
    };
    auto addCable = [this](const QString& name) {
        auto* it = new QListWidgetItem(name);
        if (name == m_current) {
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }
        m_list->addItem(it);
    };

    if (!filtering) {
        const QStringList recents = CableCatalog::pruneRecents(AppConfig::get().recentCables, m_names);
        if (!recents.isEmpty()) {
            addHeader(tr("Recent"));
            for (const QString& n : recents)
                addCable(n);
            addHeader(tr("All cables"));
        }
    }
    for (const QString& n : CableCatalog::filterNames(m_names, m_search->text()))
        addCable(n);
}

void CablePickerDialog::choose(QListWidgetItem* item)
{
    if (!m_picked.isEmpty() || !(item->flags() & Qt::ItemIsSelectable))
        return;
    m_picked = item->text();
    CableCatalog::pushRecent(AppConfig::get().recentCables, m_picked);
    accept();
}

QString CablePickerDialog::pick(QWidget* parent, const QStringList& names, const QString& current)
{
    CablePickerDialog dlg(parent, names, current);
    return dlg.exec() == QDialog::Accepted ? dlg.m_picked : QString();
}
