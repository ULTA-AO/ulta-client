#ifndef ULTA_UPDATER_H
#define ULTA_UPDATER_H

// Checks for a newer ULTA AO release, downloads it and hands it to the small
// updater program. The release list is a signed file (latest.json plus
// latest.json.sig, an Ed25519 signature). Nothing is downloaded or run unless
// the signature matches the public key built into the client.

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class QFile;
class QNetworkReply;

class UltaUpdater : public QObject {
  Q_OBJECT

public:
  explicit UltaUpdater(QObject *parent = nullptr);

  // True when an update address was built into this client.
  static bool enabled();
  // "1.0.10" is newer than "1.0.9"; a suffix such as "-beta" is ignored.
  static bool isNewer(const QString &remote, const QString &local);
  // Checks a signature over a manifest (hex signature, raw manifest bytes).
  static bool verifySignature(const QByteArray &manifest, const QString &sig_hex);

  void check();
  void install();
  QString newVersion() const { return m_version; }
  // The release is marked required: playing needs the update first.
  bool required() const { return m_required; }

signals:
  void updateAvailable(const QString &version, const QString &notes);
  void upToDate();
  void checkFailed(const QString &why);
  void progress(int percent);
  void installFailed(const QString &why);
  void restarting();

private:
  QNetworkAccessManager m_net;
  QByteArray m_manifest;
  QString m_sig;
  bool m_manifest_ready = false;
  bool m_sig_ready = false;
  QString m_version;
  QString m_notes;
  bool m_required = false;
  QString m_url;
  QString m_sha256;
  qint64 m_size = 0;
  QString m_zip_path;
  QFile *m_download = nullptr;

  QNetworkReply *get(const QUrl &url);
  void manifest_part_done();
  void fail_check(const QString &why);
  void fail_install(const QString &why);
  void finish_download();
  void hand_over();
};

#endif // ULTA_UPDATER_H
