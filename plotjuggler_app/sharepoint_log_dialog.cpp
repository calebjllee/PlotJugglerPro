/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "sharepoint_log_dialog.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

namespace PJ
{

SharePointLogDialog::SharePointLogDialog(const QStringList& supported_extensions, QWidget* parent)
  : QDialog(parent), _client(this), _supported_extensions(supported_extensions)
{
  setWindowTitle(tr("SharePoint Logs"));
  resize(860, 520);

  auto* main_layout = new QVBoxLayout(this);
  auto* lists_layout = new QHBoxLayout;

  auto* folder_panel = new QVBoxLayout;
  folder_panel->addWidget(new QLabel(tr("Date folders"), this));
  _folder_list = new QListWidget(this);
  folder_panel->addWidget(_folder_list);

  auto* file_panel = new QVBoxLayout;
  file_panel->addWidget(new QLabel(tr("Log files"), this));
  _file_list = new QListWidget(this);
  _file_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
  file_panel->addWidget(_file_list);

  lists_layout->addLayout(folder_panel, 1);
  lists_layout->addLayout(file_panel, 2);
  main_layout->addLayout(lists_layout);

  auto* buttons = new QDialogButtonBox(this);
  auto* refresh_button = buttons->addButton(tr("Refresh"), QDialogButtonBox::ActionRole);
  _download_button = buttons->addButton(tr("Load"), QDialogButtonBox::AcceptRole);
  buttons->addButton(QDialogButtonBox::Cancel);
  _download_button->setEnabled(false);
  main_layout->addWidget(buttons);

  connect(refresh_button, &QPushButton::clicked, this, [this] { refreshFolders(); });
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(_download_button, &QPushButton::clicked, this, [this] { downloadSelected(); });
  connect(_folder_list, &QListWidget::currentRowChanged, this,
          [this](int row) { refreshFiles(row); });
  connect(_file_list, &QListWidget::itemSelectionChanged, this, [this] {
    _download_button->setEnabled(!_file_list->selectedItems().isEmpty());
  });

  refreshFolders();
}

QStringList SharePointLogDialog::selectedLocalFiles() const
{
  return _selected_local_files;
}

void SharePointLogDialog::refreshFolders()
{
  _folder_list->clear();
  _file_list->clear();
  _folders.clear();
  _files.clear();
  _download_button->setEnabled(false);

  QString error;
  if (!_client.prepare(&error))
  {
    showError(error);
    return;
  }
  _folders = _client.listDateFolders(&error);
  if (!_folders.isEmpty())
  {
    for (const auto& folder : _folders)
    {
      _folder_list->addItem(folder.name);
    }
    _folder_list->setCurrentRow(0);
  }
  else if (!error.isEmpty())
  {
    showError(error);
  }
}

void SharePointLogDialog::refreshFiles(int row)
{
  _file_list->clear();
  _files.clear();
  _download_button->setEnabled(false);
  if (row < 0 || row >= _folders.size())
  {
    return;
  }

  QString error;
  _files = _client.listFiles(_folders[row], _supported_extensions, &error);
  if (!_files.isEmpty())
  {
    for (const auto& file : _files)
    {
      const QString size =
          file.size > 0 ? QString("  (%1 MB)").arg(file.size / (1024.0 * 1024.0), 0, 'f', 1)
                        : QString();
      _file_list->addItem(file.name + size);
    }
  }
  else if (!error.isEmpty())
  {
    showError(error);
  }
}

void SharePointLogDialog::downloadSelected()
{
  const int folder_row = _folder_list->currentRow();
  if (folder_row < 0 || folder_row >= _folders.size())
  {
    return;
  }

  _selected_local_files.clear();
  for (QListWidgetItem* item : _file_list->selectedItems())
  {
    const int row = _file_list->row(item);
    if (row < 0 || row >= _files.size())
    {
      continue;
    }
    QString error;
    const QString local_path = _client.downloadFile(_folders[folder_row], _files[row], &error);
    if (local_path.isEmpty())
    {
      showError(error);
      return;
    }
    _selected_local_files.push_back(local_path);
  }

  if (!_selected_local_files.isEmpty())
  {
    accept();
  }
}

void SharePointLogDialog::showError(const QString& message)
{
  QMessageBox msg(this);
  msg.setIcon(QMessageBox::Warning);
  msg.setWindowTitle(tr("SharePoint Logs"));
  msg.setText(message.isEmpty() ? tr("SharePoint request failed.") : message);
  msg.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard |
                              Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
  msg.exec();
}

}  // namespace PJ
