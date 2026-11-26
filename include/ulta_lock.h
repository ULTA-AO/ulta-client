#ifndef ULTA_LOCK_H
#define ULTA_LOCK_H

// ULTA locked-server build. Configure with
//   -DULTA_LOCKED_SERVER=host:port  -DULTA_LOCKED_NAME="ULTA AO"
// to bake in the one server this client may talk to (TCP). Without the flag
// the client is the open dev variant. This is a SOFT lock: a modified build
// can bypass it; real access control stays on the server.
#ifdef ULTA_LOCKED_SERVER

#include <QString>

namespace UltaLock {
inline constexpr bool enabled = true;
inline QString host()
{
  const QString s = QStringLiteral(ULTA_LOCKED_SERVER);
  return s.left(s.lastIndexOf(':'));
}
inline int port()
{
  const QString s = QStringLiteral(ULTA_LOCKED_SERVER);
  return s.mid(s.lastIndexOf(':') + 1).toInt();
}
inline QString name()
{
#ifdef ULTA_LOCKED_NAME
  return QStringLiteral(ULTA_LOCKED_NAME);
#else
  return QStringLiteral("ULTA AO");
#endif
}
inline bool allows(const QString &p_host, int p_port)
{
  return p_host == host() && p_port == port();
}
} // namespace UltaLock

#else

#include <QString>

// Open build: same API so call sites compile either way; never used at runtime
// because `enabled` is false.
namespace UltaLock {
inline constexpr bool enabled = false;
inline QString host() { return QString(); }
inline int port() { return 0; }
inline QString name() { return QString(); }
inline bool allows(const QString &, int) { return true; }
}

#endif
#endif // ULTA_LOCK_H
