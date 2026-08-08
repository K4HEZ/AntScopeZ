#include "editbandsdialog.h"
#include "ui_editbandsdialog.h"
#include <QAbstractButton>
#include <QFile>
#include "settings.h"
#include "style.h"

extern int g_showMessageBox(QWidget* parent, QMessageBox::Icon icon,
                            QString title, QString text,
                            QMessageBox::StandardButtons buttons = QMessageBox::Ok,
                            QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

EditBandsDialog::EditBandsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::EditBandsDialog)
{
    ui->setupUi(this);

    setStyleSheet(Style::dialog());

    QFont font = ui->textEdit->font();
    font.setPointSize(12);
    ui->textEdit->setFont(font);

    connect(ui->buttonBox, &QDialogButtonBox::clicked, [=](QAbstractButton* _button){
        QPushButton* button = qobject_cast<QPushButton*>(_button);
        if(button == ui->buttonBox->button(QDialogButtonBox::RestoreDefaults)) {
            loadDefaults();
        } else if(button == ui->buttonBox->button(QDialogButtonBox::Save)) {
            save();
            QDialog::accept();
        } else if(button == ui->buttonBox->button(QDialogButtonBox::Cancel)) {
            QDialog::reject();
        }
    });
    load();
}

EditBandsDialog::~EditBandsDialog()
{
    delete ui;
}

void EditBandsDialog::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        ui->retranslateUi(this);
        break;
    default:
        break;
    }
}

bool EditBandsDialog::loadDefaults()
{
    QString ituPath = Settings::programDataPath("itu-regions-defaults.txt");

    QFile file(ituPath);
    bool res = file.open(QFile::ReadOnly);
    if(!res) {
        qDebug() << "load defaults" << file.errorString() << ituPath;
        // Only reachable if the shipped, read-only itu-regions-defaults.txt
        // itself can't be opened (missing/unreadable/broken install) --
        // triggered by the "Restore Defaults" button, not normal use.
        g_showMessageBox(this, QMessageBox::Information, tr("Couldn't load default bands"), file.errorString() + ituPath);
        return false;
    }

    m_filePath = ituPath;

    ui->textEdit->clear();
    QTextStream stream(&file);
    ui->textEdit->setText(stream.readAll());
    file.close();

    return true;
}

bool EditBandsDialog::load()
{
    QString ituPath = Settings::localDataPath("itu-regions.txt");
    QFile file(ituPath);
    if (!file.exists()) {
        file.setFileName(Settings::programDataPath("itu-regions-defaults.txt"));
    }
    bool res = file.open(QFile::ReadOnly);
    if(!res) {
        qDebug() << "load" << file.errorString() << ituPath;
        // Only reachable if both itu-regions.txt (the user's own edits, if
        // any) and the itu-regions-defaults.txt fallback fail to open.
        g_showMessageBox(this, QMessageBox::Information, tr("Couldn't load bands"), file.errorString() + ituPath);
        return false;
    }

    m_filePath = ituPath;

    ui->textEdit->clear();
    QTextStream stream(&file);
    ui->textEdit->setText(stream.readAll());
    file.close();

    return true;
}

bool EditBandsDialog::save()
{
    QString ituPath = Settings::localDataPath("itu-regions.txt");
    QFile file(ituPath);
    bool res = file.open(QFile::Truncate|QFile::WriteOnly|QFile::Text);
    if(!res) {
        qDebug() << "save" << file.errorString() << ituPath;
        // Only reachable if writing itu-regions.txt fails (e.g. a
        // permissions problem), triggered by the "Save" button.
        g_showMessageBox(this, QMessageBox::Information, tr("Couldn't save bands"), file.errorString() + ituPath);
        return false;
    }
    QTextStream stream(&file);
    stream << ui->textEdit->toPlainText();

    file.flush();
    file.close();
    m_changed = true;

    return true;
}

