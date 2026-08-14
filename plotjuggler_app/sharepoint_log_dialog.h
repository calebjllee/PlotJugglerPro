/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SHAREPOINT_LOG_DIALOG_H
#define SHAREPOINT_LOG_DIALOG_H

#include "sharepoint_graph_client.h"

#include <QDialog>
#include <QStringList>

class QListWidget;
class QPushButton;

namespace PJ
{

class SharePointLogDialog : public QDialog
{
public:
  explicit SharePointLogDialog(const QStringList& supported_extensions, QWidget* parent = nullptr);

  QStringList selectedLocalFiles() const;

private:
  SharePointGraphClient _client;
  QStringList _supported_extensions;
  QList<SharePointDriveItem> _folders;
  QList<SharePointDriveItem> _files;
  QStringList _selected_local_files;

  QListWidget* _folder_list = nullptr;
  QListWidget* _file_list = nullptr;
  QPushButton* _download_button = nullptr;

  void refreshFolders();
  void refreshFiles(int row);
  void downloadSelected();
  void showError(const QString& message);
};

}  // namespace PJ

#endif  // SHAREPOINT_LOG_DIALOG_H
