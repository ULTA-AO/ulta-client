#include "ulta_updater.h"

#include "ulta_update_key.h"
#include "ulta_version.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QUrl>

extern "C" {
#include "../third_party/tweetnacl/tweetnacl.h"

// TweetNaCl needs this symbol to link, but the client only verifies signatures
// and never creates keys, so any call to it is a bug.
void randombytes(unsigned char *, unsigned long long)
{
  qFatal("randombytes called in the update code");
}
}

#ifdef ULTA_UPDATE_URL
static const char *kUpdateUrl = ULTA_UPDATE_URL;
#else
static const char *kUpdateUrl = "";
#endif

UltaUpdater::UltaUpdater(QObject *parent) : QObject(parent) {}

bool UltaUpdater::enabled()
{
  return kUpdateUrl[0] != '\0';
}

bool UltaUpdater::isNewer(const QString &remote, const QString &local)
{
  auto parts = [](const QString &v) {
    QList<int> out;
    const QStringList pieces = v.section('-', 0, 0).split('.');
    for (const QString &p : pieces)
      out.append(p.toInt());
    while (out.size() < 3)
      out.append(0);
    return out;
  };
  const QList<int> a = parts(remote);
  const QList<int> b = parts(local);
  for (int i = 0; i < 3; ++i) {
    if (a.at(i) != b.at(i))
      return a.at(i) > b.at(i);
  }
  return false;
}

bool UltaUpdater::verifySignature(const QByteArray &manifest, const QString &sig_hex)
{
  const QByteArray sig = QByteArray::fromHex(sig_hex.trimmed().toLatin1());
  const QByteArray key = QByteArray::fromHex(QByteArray(ULTA_UPDATE_PUBLIC_KEY));
  if (sig.size() != crypto_sign_BYTES || key.size() != crypto_sign_PUBLICKEYBYTES)
    return false;
  // TweetNaCl checks "signature followed by message" in one buffer.
  QByteArray signed_message = sig + manifest;
  QByteArray opened(signed_message.size(), 0);
  unsigned long long opened_len = 0;
  const int result = crypto_sign_open(
      reinterpret_cast<unsigned char *>(opened.data()), &opened_len,
      reinterpret_cast<const unsigned char *>(signed_message.constData()),
      static_cast<unsigned long long>(signed_message.size()),
      reinterpret_cast<const unsigned char *>(key.constData()));
  return result == 0 && opened_len == static_cast<unsigned long long>(manifest.size());
}

QNetworkReply *UltaUpdater::get(const QUrl &url)
{
  QNetworkRequest request(url);
  request.setAttribute(QNetworkRequest::FollowRedirectsAttribute, true);
  request.setHeader(QNetworkRequest::UserAgentHeader, QString("ULTA-AO/%1").arg(ulta_client_version()));
  return m_net.get(request);
}

void UltaUpdater::fail_check(const QString &why)
{
  qWarning() << "update check:" << why;
  emit checkFailed(why);
}

void UltaUpdater::check()
{
  if (!enabled())
    return;
  m_manifest_ready = m_sig_ready = false;
  const QUrl url{QString::fromLatin1(kUpdateUrl)};
#ifndef ULTA_UPDATE_ALLOW_HTTP
  if (url.scheme() != "https") {
    fail_check("the update address must use https");
    return;
  }
#endif
  QNetworkReply *manifest = get(url);
  connect(manifest, &QNetworkReply::finished, this, [this, manifest] {
    manifest->deleteLater();
    if (manifest->error() != QNetworkReply::NoError) {
      fail_check(manifest->errorString());
      return;
    }
    m_manifest = manifest->readAll();
    m_manifest_ready = true;
    manifest_part_done();
  });
  QNetworkReply *sig = get(QUrl(url.toString() + ".sig"));
  connect(sig, &QNetworkReply::finished, this, [this, sig] {
    sig->deleteLater();
    if (sig->error() != QNetworkReply::NoError) {
      fail_check(sig->errorString());
      return;
    }
    m_sig = QString::fromLatin1(sig->readAll());
    m_sig_ready = true;
    manifest_part_done();
  });
}

void UltaUpdater::manifest_part_done()
{
  if (!m_manifest_ready || !m_sig_ready)
    return;
  if (!verifySignature(m_manifest, m_sig)) {
    fail_check("the update information is not signed by ULTA AO");
    return;
  }
  const QJsonObject json = QJsonDocument::fromJson(m_manifest).object();
  m_version = json.value("version").toString();
  m_url = json.value("url").toString();
  m_sha256 = json.value("sha256").toString().toLower();
  m_size = static_cast<qint64>(json.value("size").toDouble());
  m_notes = json.value("notes").toString();
  m_required = json.value("required").toBool(false);
  if (m_version.isEmpty() || m_url.isEmpty() || m_sha256.size() != 64 || m_size <= 0) {
    fail_check("the update information is incomplete");
    return;
  }
  if (isNewer(m_version, ulta_client_version()))
    emit updateAvailable(m_version, m_notes);
  else
    emit upToDate();
}

void UltaUpdater::fail_install(const QString &why)
{
  qWarning() << "update install:" << why;
  if (m_download) {
    m_download->close();
    m_download->remove();
    m_download->deleteLater();
    m_download = nullptr;
  }
  emit installFailed(why);
}

void UltaUpdater::install()
{
#ifndef Q_OS_WIN
  fail_install("automatic updates are not available on this system yet");
  return;
#else
  if (m_version.isEmpty())
    return;
  const QUrl url(m_url);
#ifndef ULTA_UPDATE_ALLOW_HTTP
  if (url.scheme() != "https") {
    fail_install("the download address must use https");
    return;
  }
#endif
  const QString folder = QCoreApplication::applicationDirPath() + "/update";
  if (!QDir().mkpath(folder)) {
    fail_install("the install folder is not writable");
    return;
  }
  m_zip_path = folder + QString("/ULTA-AO-%1.zip").arg(m_version);
  m_download = new QFile(m_zip_path, this);
  if (!m_download->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    fail_install("could not create the download file");
    return;
  }
  QNetworkReply *reply = get(url);
  connect(reply, &QNetworkReply::readyRead, this, [this, reply] {
    if (m_download)
      m_download->write(reply->readAll());
  });
  connect(reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 total) {
    const qint64 expected = total > 0 ? total : m_size;
    if (expected > 0)
      emit progress(static_cast<int>(got * 100 / expected));
  });
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    if (!m_download)
      return;
    if (reply->error() != QNetworkReply::NoError) {
      fail_install(reply->errorString());
      return;
    }
    m_download->write(reply->readAll());
    finish_download();
  });
#endif
}

void UltaUpdater::finish_download()
{
  m_download->close();
  // The signed manifest names the size and the SHA-256 of the file.
  QFile file(m_zip_path);
  if (!file.open(QIODevice::ReadOnly) || file.size() != m_size) {
    fail_install("the downloaded file has the wrong size");
    return;
  }
  QCryptographicHash hash(QCryptographicHash::Sha256);
  if (!hash.addData(&file)) {
    fail_install("could not check the downloaded file");
    return;
  }
  file.close();
  if (hash.result().toHex().toLower() != m_sha256.toLatin1()) {
    fail_install("the downloaded file does not match the signed update information");
    return;
  }
  m_download->deleteLater();
  m_download = nullptr;
  hand_over();
}

void UltaUpdater::hand_over()
{
#ifdef Q_OS_WIN
  const QString app_dir = QCoreApplication::applicationDirPath();
  const QString helper = app_dir + "/ulta-updater.exe";
  if (!QFileInfo::exists(helper)) {
    fail_install("the updater program is missing");
    return;
  }
  // Run a copy from the temp folder, so the updater can also replace itself.
  const QString copy = QDir::temp().filePath(
      QString("ulta-updater-%1.exe").arg(QCoreApplication::applicationPid()));
  QFile::remove(copy);
  if (!QFile::copy(helper, copy)) {
    fail_install("could not start the updater");
    return;
  }
  const QStringList args = {"--zip", m_zip_path, "--dir", app_dir, "--pid",
                            QString::number(QCoreApplication::applicationPid()),
                            "--exe", QFileInfo(QCoreApplication::applicationFilePath()).fileName()};
  QProcess process;
  process.setProgram(copy);
  process.setArguments(args);
  process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
    arguments->flags |= 0x08000000; // CREATE_NO_WINDOW
  });
  if (!process.startDetached()) {
    fail_install("could not start the updater");
    return;
  }
  emit restarting();
#endif
}
