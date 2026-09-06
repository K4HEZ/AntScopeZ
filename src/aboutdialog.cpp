#include "aboutdialog.h"
#include "ui_aboutdialog.h"
#include "build-timestamp.h"
#include <QResizeEvent>
#include <QShowEvent>

AboutDialog::AboutDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AboutDialog)
{
    ui->setupUi(this);

    // Same "Version:" label/value pair the Settings > Updates tab shows
    // (Settings::setAntScopeVersion()) -- kept visually consistent rather
    // than introducing a second way to display the app version.
    ui->versionLabel->setText(ANTSCOPEZ_VER);

    // ANTSCOPEZ_BUILD_TIMESTAMP (build-timestamp.h, generated fresh every
    // build -- see CMakeLists.txt) rather than this file's own compile
    // time: aboutdialog.cpp only recompiles when it or something it
    // includes changes, which would make a plain __DATE__/__TIME__ here go
    // stale across incremental rebuilds that touch other files.
    ui->buildLabel->setText(tr("Build: ") + ANTSCOPEZ_BUILD_TIMESTAMP);

    updateFlagLabelWidths();
}

AboutDialog::~AboutDialog()
{
    delete ui;
}

void AboutDialog::resizeEvent(QResizeEvent *event)
{
    QDialog::resizeEvent(event);
    updateFlagLabelWidths();
}

void AboutDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    // creditsLabel's width isn't final until the layout's actually been
    // activated by showing the dialog -- constructor-time width would be
    // whatever Designer's placeholder geometry happened to be.
    updateFlagLabelWidths();
}

void AboutDialog::updateFlagLabelWidths()
{
    // 80% of creditsLabel's current width, not a hardcoded pixel size --
    // scales with the dialog instead of drifting out of proportion with
    // the credits text next to it.
    int width = qRound(ui->creditsLabel->width() * 0.8);
    ui->label_2->setFixedWidth(width);
    ui->label_3->setFixedWidth(width);
}
