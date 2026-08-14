/*
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "sharepoint_graph_client.h"
#include "sharepoint_config.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressDialog>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSslSocket>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrlQuery>

namespace
{
constexpr const char* GRAPH_BASE = "https://graph.microsoft.com/v1.0";

QString encodePath(const QString& path)
{
  QStringList encoded_parts;
  for (const QString& part : path.split('/', Qt::SkipEmptyParts))
  {
    encoded_parts.push_back(QString::fromLatin1(QUrl::toPercentEncoding(part)));
  }
  return "/" + encoded_parts.join('/');
}

QString graphErrorMessage(const QByteArray& body)
{
  const QJsonDocument doc = QJsonDocument::fromJson(body);
  const QJsonObject error = doc.object().value("error").toObject();
  const QString message = error.value("message").toString();
  return message.isEmpty() ? QString::fromUtf8(body) : message;
}

QString httpFailureMessage(const QString& operation, int status_code, const QByteArray& body,
                           const QString& network_error)
{
  const QString detail = body.isEmpty() ? network_error : graphErrorMessage(body);
  if (status_code > 0)
  {
    return QObject::tr("%1 failed (%2): %3")
        .arg(operation)
        .arg(status_code)
        .arg(detail.isEmpty() ? QObject::tr("No response details") : detail);
  }
  return QObject::tr("%1 failed: %2")
      .arg(operation)
      .arg(detail.isEmpty() ? QObject::tr("No network response") : detail);
}

QString safeFileName(QString name)
{
  static const QRegularExpression bad_chars(R"([<>:"/\\|?*])");
  name.replace(bad_chars, "_");
  return name;
}

QByteArray formBody(const QList<QPair<QString, QString>>& values)
{
  QUrlQuery query;
  for (const auto& pair : values)
  {
    query.addQueryItem(pair.first, pair.second);
  }
  return query.toString(QUrl::FullyEncoded).toUtf8();
}

bool waitWithEvents(int milliseconds, QProgressDialog* progress = nullptr,
                    QWidget* cancel_widget = nullptr)
{
  QElapsedTimer timer;
  timer.start();
  while (timer.elapsed() < milliseconds)
  {
    if ((progress && progress->wasCanceled()) || (cancel_widget && !cancel_widget->isVisible()))
    {
      return false;
    }
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    QThread::msleep(50);
  }
  return true;
}
}  // namespace

namespace PJ
{

SharePointGraphClient::SharePointGraphClient(QWidget* parent) : _parent(parent)
{
  loadTokenCache();
}

bool SharePointGraphClient::prepare(QString* error)
{
  if (!QSslSocket::supportsSsl())
  {
    if (error)
    {
      *error =
          QObject::tr("Qt TLS support is unavailable.\n\n"
                      "SSL build version: %1\n"
                      "SSL runtime version: %2\n"
                      "Application directory: %3\n\n"
                      "On Windows, copy the OpenSSL DLLs that match this Qt build "
                      "(usually libssl-1_1-x64.dll and libcrypto-1_1-x64.dll for Qt 5.15) "
                      "next to the PlotJuggler executable or add their directory to PATH.")
              .arg(QSslSocket::sslLibraryBuildVersionString().isEmpty() ?
                       QObject::tr("<unknown>") :
                       QSslSocket::sslLibraryBuildVersionString())
              .arg(QSslSocket::sslLibraryVersionString().isEmpty() ?
                       QObject::tr("<not loaded>") :
                       QSslSocket::sslLibraryVersionString())
              .arg(QCoreApplication::applicationDirPath());
    }
    return false;
  }

  if (!ensureAccessToken(error))
  {
    return false;
  }

  QString site_path = QString::fromLatin1(SharePointConfig::SITE_PATH).trimmed();
  site_path.remove(QRegularExpression("^/+"));
  if (site_path.startsWith("sites/", Qt::CaseInsensitive))
  {
    site_path = site_path.mid(6);
  }

  const QString site_url =
      QString("%1/sites/%2:/sites/%3")
          .arg(GRAPH_BASE, QString::fromLatin1(SharePointConfig::SITE_HOSTNAME),
               QString::fromLatin1(QUrl::toPercentEncoding(site_path, "/")));
  const QJsonObject site =
      graphJson(site_url, QObject::tr("Resolve SharePoint site [%1]").arg(site_url), error);
  if (site.isEmpty())
  {
    return false;
  }
  _site_id = site.value("id").toString();

  const QJsonObject drive =
      graphJson(QString("%1/sites/%2/drive").arg(GRAPH_BASE, _site_id),
                QObject::tr("Resolve SharePoint document drive"), error);
  if (drive.isEmpty())
  {
    return false;
  }
  _drive_id = drive.value("id").toString();
  return !_drive_id.isEmpty();
}

QList<SharePointDriveItem> SharePointGraphClient::listDateFolders(QString* error)
{
  QList<SharePointDriveItem> folders;
  const QString root = encodePath(QString::fromLatin1(SharePointConfig::ROOT_PATH));
  QString url =
      QString("%1/drives/%2/root:%3:/children?$select=id,name,folder,eTag,size&$top=200")
          .arg(GRAPH_BASE, _drive_id, root);
  const QString operation =
      QObject::tr("List SharePoint root folder [%1]").arg(SharePointConfig::ROOT_PATH);

  while (!url.isEmpty())
  {
    const QJsonObject page = graphJson(url, operation, error);
    if (page.isEmpty())
    {
      return {};
    }
    for (const QJsonValue& value : page.value("value").toArray())
    {
      const QJsonObject obj = value.toObject();
      if (!obj.contains("folder"))
      {
        continue;
      }
      SharePointDriveItem item;
      item.id = obj.value("id").toString();
      item.name = obj.value("name").toString();
      item.eTag = obj.value("eTag").toString();
      item.size = static_cast<qint64>(obj.value("size").toDouble());
      item.folder = true;
      folders.push_back(item);
    }
    url = page.value("@odata.nextLink").toString();
  }
  std::sort(folders.begin(), folders.end(),
            [](const SharePointDriveItem& lhs, const SharePointDriveItem& rhs) {
              return lhs.name > rhs.name;
            });
  return folders;
}

QList<SharePointDriveItem> SharePointGraphClient::listFiles(
    const SharePointDriveItem& folder, const QStringList& supported_extensions, QString* error)
{
  QList<SharePointDriveItem> files;
  QString url = QString("%1/drives/%2/items/%3/children?"
                        "$select=id,name,file,eTag,size,@microsoft.graph.downloadUrl&$top=200")
                    .arg(GRAPH_BASE, _drive_id, folder.id);
  while (!url.isEmpty())
  {
    const QJsonObject page = graphJson(url, QObject::tr("List SharePoint files in [%1]").arg(folder.name),
                                       error);
    if (page.isEmpty())
    {
      return {};
    }
    for (const QJsonValue& value : page.value("value").toArray())
    {
      const QJsonObject obj = value.toObject();
      const QString suffix = QFileInfo(obj.value("name").toString()).suffix().toLower();
      if (!obj.contains("file") || !supported_extensions.contains(suffix))
      {
        continue;
      }
      SharePointDriveItem item;
      item.id = obj.value("id").toString();
      item.name = obj.value("name").toString();
      item.eTag = obj.value("eTag").toString();
      item.downloadUrl = obj.value("@microsoft.graph.downloadUrl").toString();
      item.size = static_cast<qint64>(obj.value("size").toDouble());
      files.push_back(item);
    }
    url = page.value("@odata.nextLink").toString();
  }
  std::sort(files.begin(), files.end(),
            [](const SharePointDriveItem& lhs, const SharePointDriveItem& rhs) {
              return lhs.name < rhs.name;
            });
  return files;
}

QString SharePointGraphClient::downloadFile(const SharePointDriveItem& folder,
                                            const SharePointDriveItem& file, QString* error)
{
  const QString folder_path = cacheRootPath() + "/" + safeFileName(folder.name);
  QDir().mkpath(folder_path);
  const QString local_path = folder_path + "/" + safeFileName(file.name);
  const QString metadata_path = local_path + ".pjsharepoint.json";

  QFileInfo cached(local_path);
  QFile metadata(metadata_path);
  if (cached.isFile() && cached.size() == file.size && metadata.open(QFile::ReadOnly))
  {
    const QJsonObject obj = QJsonDocument::fromJson(metadata.readAll()).object();
    if (obj.value("eTag").toString() == file.eTag)
    {
      return local_path;
    }
  }

  QUrl download_url(file.downloadUrl);
  if (download_url.isEmpty())
  {
    download_url = QUrl(QString("%1/drives/%2/items/%3/content").arg(GRAPH_BASE, _drive_id, file.id));
  }

  QProgressDialog progress(_parent);
  progress.setWindowTitle(QObject::tr("Downloading SharePoint Log"));
  progress.setLabelText(QObject::tr("Downloading %1").arg(file.name));
  progress.setCancelButtonText(QObject::tr("Cancel"));
  progress.setRange(0, file.size > 0 ? 1000 : 0);
  progress.setWindowModality(Qt::ApplicationModal);
  progress.show();

  QNetworkRequest request(download_url);
  request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  if (download_url.host().contains("graph.microsoft.com"))
  {
    request.setRawHeader("Authorization", QString("Bearer %1").arg(_access_token).toUtf8());
  }
  if (!downloadWithRetry(request, local_path, &progress, error))
  {
    return {};
  }

  QSaveFile meta(metadata_path);
  if (meta.open(QFile::WriteOnly))
  {
    QJsonObject obj;
    obj["eTag"] = file.eTag;
    obj["size"] = QString::number(file.size);
    obj["remoteName"] = file.name;
    obj["folder"] = folder.name;
    meta.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    meta.commit();
  }
  return local_path;
}

bool SharePointGraphClient::ensureAccessToken(QString* error)
{
  const qint64 now = QDateTime::currentSecsSinceEpoch();
  if (!_access_token.isEmpty() && _expires_at > now + 120)
  {
    return true;
  }
  if (!_refresh_token.isEmpty() && refreshAccessToken(error))
  {
    return true;
  }
  return startDeviceLogin(error);
}

bool SharePointGraphClient::loadTokenCache()
{
  QFile file(tokenCachePath());
  if (!file.open(QFile::ReadOnly))
  {
    return false;
  }
  const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
  _access_token = obj.value("access_token").toString();
  _refresh_token = obj.value("refresh_token").toString();
  _expires_at = obj.value("expires_at").toString().toLongLong();
  return true;
}

void SharePointGraphClient::saveTokenCache() const
{
  QDir().mkpath(QFileInfo(tokenCachePath()).absolutePath());
  QSaveFile file(tokenCachePath());
  if (!file.open(QFile::WriteOnly))
  {
    return;
  }
  QJsonObject obj;
  obj["access_token"] = _access_token;
  obj["refresh_token"] = _refresh_token;
  obj["expires_at"] = QString::number(_expires_at);
  file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
  file.commit();
}

bool SharePointGraphClient::refreshAccessToken(QString* error)
{
  QNetworkRequest request{ QUrl(tokenEndpoint()) };
  request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  const QByteArray body = formBody({ { "client_id", SharePointConfig::CLIENT_ID },
                                     { "scope", SharePointConfig::GRAPH_SCOPE },
                                     { "refresh_token", _refresh_token },
                                     { "grant_type", "refresh_token" } });
  const HttpResult result = sendRequest(request, "POST", body);
  if (result.status_code < 200 || result.status_code >= 300)
  {
    _access_token.clear();
    _refresh_token.clear();
    return false;
  }
  const QJsonObject obj = QJsonDocument::fromJson(result.body).object();
  _access_token = obj.value("access_token").toString();
  _refresh_token = obj.value("refresh_token").toString(_refresh_token);
  _expires_at = QDateTime::currentSecsSinceEpoch() + obj.value("expires_in").toInt(3600);
  saveTokenCache();
  Q_UNUSED(error);
  return !_access_token.isEmpty();
}

bool SharePointGraphClient::startDeviceLogin(QString* error)
{
  QNetworkRequest device_request{ QUrl(deviceCodeEndpoint()) };
  device_request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
  const QByteArray device_body = formBody({ { "client_id", SharePointConfig::CLIENT_ID },
                                            { "scope", SharePointConfig::GRAPH_SCOPE } });
  const HttpResult device_result = sendRequest(device_request, "POST", device_body);
  if (device_result.status_code < 200 || device_result.status_code >= 300)
  {
    if (error)
    {
      *error = httpFailureMessage(QObject::tr("Microsoft sign-in could not start"),
                                  device_result.status_code, device_result.body,
                                  device_result.error);
    }
    return false;
  }

  const QJsonObject flow = QJsonDocument::fromJson(device_result.body).object();
  const QString device_code = flow.value("device_code").toString();
  const QString message = flow.value("message").toString();
  const QString verification_uri = flow.value("verification_uri").toString();
  int interval = flow.value("interval").toInt(5);
  const int expires_in = flow.value("expires_in").toInt(900);

  QMessageBox msg(_parent);
  msg.setWindowTitle(QObject::tr("Microsoft Sign-In"));
  msg.setText(message.isEmpty() ? QObject::tr("Complete Microsoft device sign-in in your browser.")
                                : message);
  msg.setInformativeText(QObject::tr("Waiting for Microsoft sign-in..."));
  msg.setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard |
                              Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
  msg.setStandardButtons(QMessageBox::Cancel);
  if (!verification_uri.isEmpty())
  {
    QDesktopServices::openUrl(QUrl(verification_uri));
  }
  msg.show();

  const qint64 start = QDateTime::currentSecsSinceEpoch();
  while (QDateTime::currentSecsSinceEpoch() - start < expires_in)
  {
    if (!msg.isVisible())
    {
      if (error)
      {
        *error = QObject::tr("Microsoft sign-in was cancelled");
      }
      return false;
    }
    if (!waitWithEvents(interval * 1000, nullptr, &msg))
    {
      if (error)
      {
        *error = QObject::tr("Microsoft sign-in was cancelled");
      }
      return false;
    }

    QNetworkRequest token_request{ QUrl(tokenEndpoint()) };
    token_request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    const QByteArray token_body =
        formBody({ { "grant_type", "urn:ietf:params:oauth:grant-type:device_code" },
                   { "client_id", SharePointConfig::CLIENT_ID },
                   { "device_code", device_code } });
    const HttpResult token_result = sendRequest(token_request, "POST", token_body);
    const QJsonObject obj = QJsonDocument::fromJson(token_result.body).object();
    if (token_result.status_code >= 200 && token_result.status_code < 300)
    {
      _access_token = obj.value("access_token").toString();
      _refresh_token = obj.value("refresh_token").toString(_refresh_token);
      _expires_at = QDateTime::currentSecsSinceEpoch() + obj.value("expires_in").toInt(3600);
      saveTokenCache();
      msg.accept();
      return !_access_token.isEmpty();
    }

    const QString code = obj.value("error").toString();
    if (code == "authorization_pending")
    {
      continue;
    }
    if (code == "slow_down")
    {
      interval += 5;
      continue;
    }
    if (error)
    {
      *error = QObject::tr("Microsoft sign-in failed: %1")
                   .arg(obj.value("error_description").toString(graphErrorMessage(token_result.body)));
    }
    return false;
  }

  if (error)
  {
    *error = QObject::tr("Microsoft sign-in expired");
  }
  return false;
}

QJsonObject SharePointGraphClient::graphJson(const QString& url, const QString& operation,
                                             QString* error)
{
  QNetworkRequest request{ QUrl(url) };
  request.setRawHeader("Authorization", QString("Bearer %1").arg(_access_token).toUtf8());
  request.setRawHeader("Accept", "application/json");
  const HttpResult result = sendWithRetry(request, "GET", QByteArray(), nullptr, error);
  if (result.status_code < 200 || result.status_code >= 300)
  {
    if (error && !error->isEmpty())
    {
      *error = operation + "\n\n" + *error;
    }
    return {};
  }
  return QJsonDocument::fromJson(result.body).object();
}

SharePointGraphClient::HttpResult SharePointGraphClient::sendRequest(
    const QNetworkRequest& request, const QByteArray& method, const QByteArray& body,
    QProgressDialog* progress)
{
  HttpResult result;
  QNetworkReply* reply = _network.sendCustomRequest(request, method, body);

  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  if (progress)
  {
    QObject::connect(reply, &QNetworkReply::downloadProgress, progress,
                     [progress](qint64 received, qint64 total) {
                       if (total > 0)
                       {
                         progress->setValue(static_cast<int>((received * 1000) / total));
                       }
                     });
    QObject::connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
  }
  timeout.start(120000);
  loop.exec();

  if (timeout.isActive())
  {
    timeout.stop();
  }
  else if (!reply->isFinished())
  {
    reply->abort();
    result.error = QObject::tr("Network request timed out");
  }

  result.status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  result.body = reply->readAll();
  if (reply->error() != QNetworkReply::NoError && result.error.isEmpty())
  {
    result.error = reply->errorString();
  }
  reply->deleteLater();
  return result;
}

SharePointGraphClient::HttpResult SharePointGraphClient::sendWithRetry(
    const QNetworkRequest& request, const QByteArray& method, const QByteArray& body,
    QProgressDialog* progress, QString* error)
{
  HttpResult result;
  for (int attempt = 0; attempt < 6; attempt++)
  {
    result = sendRequest(request, method, body, progress);
    if (progress && progress->wasCanceled())
    {
      return result;
    }
    if (result.status_code == 401 && refreshAccessToken(error))
    {
      QNetworkRequest retry_request(request);
      retry_request.setRawHeader("Authorization", QString("Bearer %1").arg(_access_token).toUtf8());
      result = sendRequest(retry_request, method, body, progress);
    }
    if ((result.status_code >= 200 && result.status_code < 300) ||
        !QSet<int>({ 0, 429, 500, 502, 503, 504 }).contains(result.status_code))
    {
      break;
    }
    const int delay_ms = qMin(8000, 500 * (1 << attempt));
    waitWithEvents(delay_ms, progress);
  }

  if (result.status_code < 200 || result.status_code >= 300)
  {
    if (error)
    {
      *error = httpFailureMessage(QObject::tr("Microsoft Graph request"), result.status_code,
                                  result.body, result.error);
    }
  }
  return result;
}

bool SharePointGraphClient::downloadWithRetry(const QNetworkRequest& request,
                                              const QString& local_path,
                                              QProgressDialog* progress, QString* error)
{
  HttpResult result;
  for (int attempt = 0; attempt < 6; attempt++)
  {
    result = downloadOnce(request, local_path, progress);
    if (progress && progress->wasCanceled())
    {
      if (error)
      {
        *error = QObject::tr("Download cancelled");
      }
      return false;
    }
    if (result.status_code == 401 && refreshAccessToken(error))
    {
      QNetworkRequest retry_request(request);
      retry_request.setRawHeader("Authorization", QString("Bearer %1").arg(_access_token).toUtf8());
      result = downloadOnce(retry_request, local_path, progress);
    }
    if ((result.status_code >= 200 && result.status_code < 300) ||
        !QSet<int>({ 0, 429, 500, 502, 503, 504 }).contains(result.status_code))
    {
      break;
    }
    const int delay_ms = qMin(8000, 500 * (1 << attempt));
    waitWithEvents(delay_ms, progress);
  }

  if (result.status_code >= 200 && result.status_code < 300)
  {
    return true;
  }
  if (error)
  {
    *error = httpFailureMessage(QObject::tr("Download"), result.status_code, result.body,
                                result.error);
  }
  return false;
}

SharePointGraphClient::HttpResult SharePointGraphClient::downloadOnce(
    const QNetworkRequest& request, const QString& local_path, QProgressDialog* progress)
{
  HttpResult result;
  QSaveFile output(local_path);
  if (!output.open(QFile::WriteOnly))
  {
    result.error = QObject::tr("Cannot write %1: %2").arg(local_path, output.errorString());
    return result;
  }

  QNetworkReply* reply = _network.get(request);
  QEventLoop loop;
  QTimer timeout;
  timeout.setSingleShot(true);
  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);
  QObject::connect(reply, &QNetworkReply::readyRead, &output, [&] { output.write(reply->readAll()); });
  if (progress)
  {
    QObject::connect(reply, &QNetworkReply::downloadProgress, progress,
                     [progress](qint64 received, qint64 total) {
                       if (total > 0)
                       {
                         progress->setValue(static_cast<int>((received * 1000) / total));
                       }
                     });
    QObject::connect(progress, &QProgressDialog::canceled, reply, &QNetworkReply::abort);
  }

  timeout.start(120000);
  loop.exec();

  output.write(reply->readAll());
  if (timeout.isActive())
  {
    timeout.stop();
  }
  else if (!reply->isFinished())
  {
    reply->abort();
    result.error = QObject::tr("Network request timed out");
  }

  result.status_code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  result.body = reply->readAll();
  if (reply->error() != QNetworkReply::NoError && result.error.isEmpty())
  {
    result.error = reply->errorString();
  }
  reply->deleteLater();

  if (result.status_code >= 200 && result.status_code < 300 && result.error.isEmpty())
  {
    if (!output.commit())
    {
      result.status_code = 0;
      result.error = QObject::tr("Cannot save %1: %2").arg(local_path, output.errorString());
    }
  }
  else
  {
    output.cancelWriting();
  }
  return result;
}

QString SharePointGraphClient::tokenEndpoint() const
{
  return QString("https://login.microsoftonline.com/%1/oauth2/v2.0/token")
      .arg(SharePointConfig::TENANT_ID);
}

QString SharePointGraphClient::deviceCodeEndpoint() const
{
  return QString("https://login.microsoftonline.com/%1/oauth2/v2.0/devicecode")
      .arg(SharePointConfig::TENANT_ID);
}

QString SharePointGraphClient::tokenCachePath() const
{
  QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (base.isEmpty())
  {
    base = QDir::homePath() + "/.plotjugglerpro";
  }
  return base + "/sharepoint_token_cache.json";
}

QString SharePointGraphClient::cacheRootPath() const
{
  QString base = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
  if (base.isEmpty())
  {
    base = QDir::homePath() + "/.plotjugglerpro";
  }
  return base + "/SharePointLogs";
}

}  // namespace PJ
