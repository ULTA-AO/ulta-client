#include "file_functions.h"

bool file_exists(QString file_path)
{
  if (file_path.isEmpty())
    return false;

  QFileInfo check_file(file_path);

  return check_file.exists() && check_file.isFile();
}

bool dir_exists(QString dir_path)
{
  if (dir_path == "")
      return false;

  QDir check_dir(dir_path);

  return check_dir.exists();
}

bool exists(QString p_path)
{
  QFile file(p_path);

  return file.exists();
}

QString get_base_path()
{
  QString base_path = "";
#if defined(__APPLE__)
  base_path = QCoreApplication::applicationDirPath() + "/../../../base/";
#else
  base_path = QCoreApplication::applicationDirPath() + "/base/";
#endif
  return base_path;
}

QString get_ulta_builtin_path()
{
  // Next to the executable (next to the .app on macOS), in "ulta/base/".
#if defined(__APPLE__)
  return QCoreApplication::applicationDirPath() + "/../../../ulta/base/";
#else
  return QCoreApplication::applicationDirPath() + "/ulta/base/";
#endif
}
