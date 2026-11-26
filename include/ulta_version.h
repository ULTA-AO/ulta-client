#ifndef ULTA_VERSION_H
#define ULTA_VERSION_H

// ULTA AO client version, set with -DULTA_VERSION=x.y.z in CMake.
#include <QString>

#ifndef ULTA_CLIENT_VERSION
#define ULTA_CLIENT_VERSION "1.0.0"
#endif

// Newest theme marker version (ulta_theme.ini). Raise it when a new widget needs a theme position
inline constexpr int ULTA_THEME_VERSION = 1;

inline QString ulta_client_version()
{
  return QStringLiteral(ULTA_CLIENT_VERSION);
}

#endif // ULTA_VERSION_H
