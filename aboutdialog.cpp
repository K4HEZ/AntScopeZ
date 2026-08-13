#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "style.h"

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    QString style = Style::dialog();
    style += Style::label();
    style += Style::pushButton();
    setStyleSheet(style);

    // Same "Version:" label/value pair the Settings > Updates tab shows
    // (Settings::setAntScopeVersion()) -- kept visually consistent rather
    // than introducing a second way to display the app version.
    ui->versionLabel->setText(ANTSCOPEZ_VER);
}

AboutDialog::~AboutDialog()
{
    delete ui;
}
