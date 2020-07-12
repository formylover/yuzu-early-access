#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QHash>
#include <QListWidgetItem>
#include <QSignalBlocker>
#include "core/settings.h"
#include "ui_configure.h"
#include "yuzu/configuration/config.h"
#include "yuzu/configuration/configure_dialog.h"
#include "yuzu/configuration/configure_input_player.h"
#include "yuzu/hotkeys.h"

ConfigureDialog::ConfigureDialog(QWidget* parent, HotkeyRegistry& registry)
    : QDialog(parent), ui(new Ui::ConfigureDialog), registry(registry) {
    Settings::configuring_global = true;

    ui->setupUi(this);
    ui->hotkeysTab->Populate(registry);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    SetConfiguration();
    PopulateSelectionList();

    connect(ui->selectorList, &QListWidget::itemSelectionChanged, this,
            &ConfigureDialog::UpdateVisibleTabs);

    adjustSize();
    ui->selectorList->setCurrentRow(0);
}

ConfigureDialog::~ConfigureDialog() = default;

void ConfigureDialog::SetConfiguration() {}

void ConfigureDialog::ApplyConfiguration() {
    ui->generalTab->ApplyConfiguration();
    ui->uiTab->ApplyConfiguration();
    ui->systemTab->ApplyConfiguration();
    ui->profileManagerTab->ApplyConfiguration();
    ui->filesystemTab->applyConfiguration();
    ui->inputTab->ApplyConfiguration();
    ui->hotkeysTab->ApplyConfiguration(registry);
    ui->cpuTab->ApplyConfiguration();
    ui->graphicsTab->ApplyConfiguration();
    ui->graphicsAdvancedTab->ApplyConfiguration();
    ui->audioTab->ApplyConfiguration();
    ui->debugTab->ApplyConfiguration();
    ui->webTab->ApplyConfiguration();
    ui->serviceTab->ApplyConfiguration();
    Settings::Apply();
    Settings::LogSettings();
}

void ConfigureDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        RetranslateUI();
    }

    QDialog::changeEvent(event);
}

void ConfigureDialog::RetranslateUI() {
    const int old_row = ui->selectorList->currentRow();
    const int old_index = ui->tabWidget->currentIndex();

    ui->retranslateUi(this);

    PopulateSelectionList();
    ui->selectorList->setCurrentRow(old_row);

    UpdateVisibleTabs();
    ui->tabWidget->setCurrentIndex(old_index);
}

Q_DECLARE_METATYPE(QList<QWidget*>);

void ConfigureDialog::PopulateSelectionList() {
    const std::array<std::pair<QString, QList<QWidget*>>, 5> items{
        {{tr("通用"), {ui->generalTab, ui->webTab, ui->debugTab, ui->uiTab}},
         {tr("系统"), {ui->systemTab, ui->profileManagerTab, ui->serviceTab, ui->filesystemTab}},
         {tr("CPU"), {ui->cpuTab}},
         {tr("图形"), {ui->graphicsTab, ui->graphicsAdvancedTab}},
         {tr("声音"), {ui->audioTab}},
         {tr("控制"), {ui->inputTab, ui->hotkeysTab}}},
    };

    [[maybe_unused]] const QSignalBlocker blocker(ui->selectorList);

    ui->selectorList->clear();
    for (const auto& entry : items) {
        auto* const item = new QListWidgetItem(entry.first);
        item->setData(Qt::UserRole, QVariant::fromValue(entry.second));

        ui->selectorList->addItem(item);
    }
}

void ConfigureDialog::UpdateVisibleTabs() {
    const auto items = ui->selectorList->selectedItems();
    if (items.isEmpty()) {
        return;
    }

    const std::map<QWidget*, QString> widgets = {
        {ui->generalTab, tr("通用")},
        {ui->systemTab, tr("系统")},
        {ui->profileManagerTab, tr("简介")},
        {ui->inputTab, tr("输入")},
        {ui->hotkeysTab, tr("热键")},
        {ui->cpuTab, tr("CPU")},
        {ui->graphicsTab, tr("图形")},
        {ui->graphicsAdvancedTab, tr("高级选项")},
        {ui->audioTab, tr("声音")},
        {ui->debugTab, tr("调试")},
        {ui->webTab, tr("网络")},
        {ui->uiTab, tr("界面")},
        {ui->filesystemTab, tr("文件系统")},
        {ui->serviceTab, tr("服务")},
    };

    [[maybe_unused]] const QSignalBlocker blocker(ui->tabWidget);

    ui->tabWidget->clear();

    const QList<QWidget*> tabs = qvariant_cast<QList<QWidget*>>(items[0]->data(Qt::UserRole));

    for (const auto tab : tabs) {
        ui->tabWidget->addTab(tab, widgets.at(tab));
    }
}
