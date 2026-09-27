#include "correctionsdialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

static QLabel* heading(const QString& text)
{
    QLabel* l = new QLabel(text);
    QFont f = l->font();
    f.setBold(true);
    l->setFont(f);
    return l;
}

static QLabel* note(const QString& text)
{
    QLabel* l = new QLabel(text);
    l->setWordWrap(true);
    return l;
}

CorrectionsDialog::CorrectionsDialog(const QString& name, const Corrections& shown, const Corrections& builtIn,
                                     const RfMath::CableParams& cableModel, bool oslAllowed, const QString& oslWhy,
                                     bool canRemoveBuiltIn, bool metric, QWidget* parent)
    : QDialog(parent), m_model(cableModel), m_metric(metric)
{
    setWindowTitle(tr("Corrections -- %1").arg(name));
    setMinimumWidth(420);

    QVBoxLayout* main = new QVBoxLayout(this);
    main->setSpacing(10);
    main->setContentsMargins(14, 14, 14, 14);
    main->addWidget(note(tr("Recomputed from this measurement's original points. "
                            "The analyzer's own calibration, if any, is always included.")));

    // OSL
    main->addSpacing(6);
    main->addWidget(heading(tr("OSL calibration")));
    m_osl = new QCheckBox(tr("Apply OSL calibration"));
    m_osl->setChecked(shown.osl);
    bool oslBuiltInFixed = builtIn.osl && !canRemoveBuiltIn;
    m_osl->setEnabled(!oslBuiltInFixed && (oslAllowed || shown.osl));
    main->addWidget(m_osl);
    QString oslText;
    if (oslBuiltInFixed)
        oslText = tr("Built into the saved points; it can't be removed.");
    else if (!oslAllowed && !shown.osl)
        oslText = oslWhy;
    else
        oslText = tr("Only correct if the test setup hasn't changed since this measurement was taken.");
    main->addWidget(note(oslText));

    // Cable
    main->addSpacing(6);
    main->addWidget(heading(tr("Cable")));
    m_cableLocked = builtIn.cableMode != 0 && !canRemoveBuiltIn;
    QFormLayout* form = new QFormLayout;
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(8);
    m_cableMode = new QComboBox;
    m_cableMode->addItems({tr("None"), tr("Subtract cable"), tr("Add cable")});
    m_cableMode->setCurrentIndex(shown.cableMode);
    m_cableMode->setEnabled(!m_cableLocked);
    form->addRow(tr("Mode:"), m_cableMode);
    m_length = new QDoubleSpinBox;
    m_length->setRange(0, 100000);
    m_length->setDecimals(2);
    m_length->setSuffix(metric ? tr(" m") : tr(" ft"));
    m_length->setValue(metric ? m_model.lengthFeet / FEETINMETER : m_model.lengthFeet);
    form->addRow(tr("Length:"), m_length);
    m_velFactor = new QDoubleSpinBox;
    m_velFactor->setRange(0.01, 1.0);
    m_velFactor->setDecimals(3);
    m_velFactor->setSingleStep(0.01);
    m_velFactor->setValue(m_model.velFactor);
    form->addRow(tr("Velocity factor:"), m_velFactor);
    main->addLayout(form);
    m_cableNote = note(QString());
    main->addWidget(m_cableNote);
    connect(m_cableMode, &QComboBox::currentIndexChanged, this, &CorrectionsDialog::updateCableFields);
    updateCableFields();

    main->addSpacing(6);
    m_copy = new QCheckBox(tr("Keep the original and add a corrected copy"));
    main->addWidget(m_copy);

    main->addStretch(1);
    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Apply)->setDefault(true);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    main->addWidget(buttons);
}

void CorrectionsDialog::updateCableFields()
{
    bool on = m_cableMode->currentIndex() != 0 && !m_cableLocked;
    m_length->setEnabled(on);
    m_velFactor->setEnabled(on);
    if (m_cableLocked) {
        m_cableNote->setText(tr("A cable correction is built into the saved points; it can't be changed."));
        return;
    }
    m_cableNote->setText(tr("R0 %1 ohm and the loss figures come from the cable model "
                            "(Settings > Cable when this measurement had none).")
                             .arg(m_model.resistance, 0, 'f', 1));
}

Corrections CorrectionsDialog::corrections() const
{
    Corrections c;
    c.osl = m_osl->isChecked();
    c.cableMode = m_cableMode->currentIndex();
    c.cable = m_model;
    c.cable.lengthFeet = m_metric ? m_length->value() * FEETINMETER : m_length->value();
    c.cable.velFactor = m_velFactor->value();
    return c;
}

bool CorrectionsDialog::asCopy() const
{
    return m_copy->isChecked();
}
