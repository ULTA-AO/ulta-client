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

// Needed to link TweetNaCl, never called (the client only verifies)
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

#ifdef Q_OS_MACOS
// Runs after the client quits: swaps in the new app, puts the old one back on any failure
static const char kMacUpdateScript[] = R"SH(#!/bin/sh
zip="$1"; app="$2"; pid="$3"
dir=$(dirname "$app"); name=$(basename "$app")
log=/tmp/ulta-update.log
echo "update $(date)" > "$log"
work=$(mktemp -d "$dir/.ulta-update.XXXXXX") || { open "$app"; exit 1; }
fail() {
  echo "failed: $1" >> "$log"
  [ -d "$work/old.app" ] && [ ! -d "$app" ] && mv "$work/old.app" "$app"
  rm -rf "$work"
  open "$app"
  exit 1
}
i=0
while kill -0 "$pid" 2>/dev/null && [ $i -lt 120 ]; do sleep 0.5; i=$((i+1)); done
/usr/bin/ditto -x -k "$zip" "$work" >> "$log" 2>&1 || fail "could not unpack the update"
new="$work/$name"
[ -d "$new" ] || fail "the update does not contain $name"
/usr/bin/xattr -dr com.apple.quarantine "$new" 2>/dev/null
/usr/bin/codesign --verify --deep "$new" >> "$log" 2>&1 || fail "the new app signature is not valid"
mv "$app" "$work/old.app" || fail "could not move the old app away"
mv "$new" "$app" || fail "could not put the new app in place"
rm -f "$zip"
rm -rf "$work"
echo "updated" >> "$log"
open "$app"
)SH";
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
  // TweetNaCl wants the signature and message in one buffer
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
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
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
  // Windows uses the top-level fields, macOS has its own entry
  QJsonObject asset = json;
#ifdef Q_OS_MACOS
  asset = json.value("macos").toObject();
  if (asset.isEmpty()) {
    emit upToDate();
    return;
  }
#endif
  m_url = asset.value("url").toString();
  m_sha256 = asset.value("sha256").toString().toLower();
  m_size = static_cast<qint64>(asset.value("size").toDouble());
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
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
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
#ifdef Q_OS_MACOS
  // The app folder must be writable, and a quarantined app runs from a read-only copy
  const QString bundle = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../..");
  if (bundle.contains("/AppTranslocation/")) {
    fail_install("move ULTA AO to your Applications folder, then open it again");
    return;
  }
  if (!QFileInfo(QFileInfo(bundle).absolutePath()).isWritable()) {
    fail_install("the folder holding ULTA AO is not writable, move the app to your Applications folder");
    return;
  }
  const QString folder = QDir::temp().filePath("ulta-update");
#else
  const QString folder = QCoreApplication::applicationDirPath() + "/update";
#endif
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
  // Check the size and SHA-256 named in the manifest
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
  // Run from a temp copy so the updater can replace itself
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
#ifdef Q_OS_MACOS
  const QString bundle = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../..");
  const QString script_path = QDir::temp().filePath(
      QString("ulta-update-%1.sh").arg(QCoreApplication::applicationPid()));
  QFile script(script_path);
  if (!script.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    fail_install("could not start the updater");
    return;
  }
  script.write(kMacUpdateScript);
  script.close();
  QFile::setPermissions(script_path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
  const QStringList args = {script_path, m_zip_path, bundle,
                            QString::number(QCoreApplication::applicationPid())};
  if (!QProcess::startDetached("/bin/sh", args)) {
    fail_install("could not start the updater");
    return;
  }
  emit restarting();
#endif
}
