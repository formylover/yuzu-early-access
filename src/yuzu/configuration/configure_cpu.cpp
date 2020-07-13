#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QComboBox>
#include <QMessageBox>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_cpu.h"
#include "yuzu/configuration/configure_cpu.h"

ConfigureCpu::ConfigureCpu(QWidget* parent) : QWidget(parent), ui(new Ui::ConfigureCpu) {
    ui->setupUi(this);

    SetConfiguration();

    connect(ui->accuracy, qOverload<int>(&QComboBox::activated), this,
            &ConfigureCpu::AccuracyUpdated);
}

ConfigureCpu::~ConfigureCpu() = default;

void ConfigureCpu::SetConfiguration() {
    const bool runtime_lock = !Core::System::GetInstance().IsPoweredOn();

    ui->accuracy->setEnabled(runtime_lock);
    ui->accuracy->setCurrentIndex(static_cast<int>(Settings::values.cpu_accuracy));
}

void ConfigureCpu::AccuracyUpdated(int index) {
    if (static_cast<Settings::CPUAccuracy>(index) == Settings::CPUAccuracy::DebugMode) {
        const auto result = QMessageBox::warning(this, tr("将CPU设置为调试模式"),
                                                 tr("CPU调试模式仅适用于开发人员使用，"
                                                    "您确定要启用此功能吗？?"),
                                                 QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::No) {
            ui->accuracy->setCurrentIndex(static_cast<int>(Settings::CPUAccuracy::Accurate));
            return;
        }
    }
}

void ConfigureCpu::ApplyConfiguration() {
    Settings::values.cpu_accuracy =
        static_cast<Settings::CPUAccuracy>(ui->accuracy->currentIndex());
}

void ConfigureCpu::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QWidget::changeEvent(event);
}

void ConfigureCpu::RetranslateUI() {
    ui->retranslateUi(this);
}
