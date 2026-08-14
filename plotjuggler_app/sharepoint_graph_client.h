/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#ifndef SHAREPOINT_GRAPH_CLIENT_H
#define SHAREPOINT_GRAPH_CLIENT_H

#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QString>
#include <QStringList>

class QProgressDialog;
class QWidget;

namespace PJ
{

struct SharePointDriveItem
{
  QString id;
  QString name;
  QString eTag;
  QString downloadUrl;
  qint64 size = 0;
  bool folder = false;
};

class SharePointGraphClient
{
public:
  explicit SharePointGraphClient(QWidget* parent);

  bool prepare(QString* error);
  QList<SharePointDriveItem> listDateFolders(QString* error);
  QList<SharePointDriveItem> listFiles(const SharePointDriveItem& folder,
                                       const QStringList& supported_extensions,
                                       QString* error);
  QString downloadFile(const SharePointDriveItem& folder, const SharePointDriveItem& file,
                       QString* error);

private:
  struct HttpResult
  {
    int status_code = 0;
    QByteArray body;
    QString error;
  };

  QWidget* _parent = nullptr;
  QNetworkAccessManager _network;
  QString _access_token;
  QString _refresh_token;
  qint64 _expires_at = 0;
  QString _site_id;
  QString _drive_id;

  bool ensureAccessToken(QString* error);
  bool loadTokenCache();
  void saveTokenCache() const;
  bool refreshAccessToken(QString* error);
  bool startDeviceLogin(QString* error);

  QJsonObject graphJson(const QString& url, const QString& operation, QString* error);
  HttpResult sendRequest(const QNetworkRequest& request, const QByteArray& method,
                         const QByteArray& body = QByteArray(),
                         QProgressDialog* progress = nullptr);
  HttpResult sendWithRetry(const QNetworkRequest& request, const QByteArray& method,
                           const QByteArray& body, QProgressDialog* progress,
                           QString* error);
  bool downloadWithRetry(const QNetworkRequest& request, const QString& local_path,
                         QProgressDialog* progress, QString* error);
  HttpResult downloadOnce(const QNetworkRequest& request, const QString& local_path,
                          QProgressDialog* progress);

  QString tokenEndpoint() const;
  QString deviceCodeEndpoint() const;
  QString tokenCachePath() const;
  QString cacheRootPath() const;
};

}  // namespace PJ

#endif  // SHAREPOINT_GRAPH_CLIENT_H
