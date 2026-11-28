
#include <QImageReader>
#include "aoapplication.h"
#ifdef Q_OS_WIN
#include <windows.h>
#endif
#include <QFileInfo>

#include "courtroom.h"
#include "datatypes.h"
#include "lobby.h"
#include "networkmanager.h"
#include "options.h"
#include "ulta_lock.h"
#include "file_functions.h"
#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QLibraryInfo>
#include <QPluginLoader>
#include <QTranslator>
#include <QResource>

// UI scale has to be known before the application object exists, so it is read
// straight from the settings file (and a --scale N argument can reset it)
static int early_ui_scale(int argc, char *argv[])
{
  QString exe_dir;
#ifdef Q_OS_WIN
  wchar_t l_path[MAX_PATH];
  const DWORD l_len = GetModuleFileNameW(nullptr, l_path, MAX_PATH);
  exe_dir = QFileInfo(QString::fromWCharArray(l_path, static_cast<int>(l_len))).absolutePath();
#else
  exe_dir = QFileInfo(QString::fromLocal8Bit(argv[0])).absolutePath();
#endif
#if defined(__APPLE__)
  QString base = exe_dir + "/../../../base/";
#else
  QString base = exe_dir + "/base/";
  if (!QDir(base).exists() && QDir(exe_dir + "/../base/").exists())
    base = QDir::cleanPath(exe_dir + "/../base") + "/";
#endif
  QSettings settings(base + "ulta_config.ini", QSettings::IniFormat);
  int percent = settings.value("ui_scale", 100).toInt();
  for (int i = 1; i + 1 < argc; ++i) {
    if (QString(argv[i]) == "--scale") {
      percent = QString(argv[i + 1]).toInt();
      settings.setValue("ui_scale", qBound(50, percent, 200));
      settings.sync();
    }
  }
  return qBound(50, percent, 200);
}

int main(int argc, char *argv[])
{
  qSetMessagePattern("%{type}: %{if-category}%{category}: %{endif}%{message}");

  const int l_ui_scale = early_ui_scale(argc, argv);
  if (l_ui_scale != 100) {
    qputenv("QT_SCALE_FACTOR", QByteArray::number(l_ui_scale / 100.0, 'f', 2));
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
  }
#ifdef Q_OS_MAC
  // Draw widgets through Metal, the default raster flush is slow on Retina
  if (qEnvironmentVariableIsEmpty("ULTA_NO_METAL")) {
    qputenv("QT_WIDGETS_RHI", "1");
    qputenv("QT_WIDGETS_RHI_BACKEND", "metal");
  }
#endif
  AOApplication main_app(argc, argv);
#ifdef Q_OS_MAC
  // Segoe UI is not on macOS, use the system font instead of a random fallback
  QFont::insertSubstitution("Segoe UI", ".AppleSystemUIFont");
#endif

  AOApplication::addLibraryPath(AOApplication::applicationDirPath() + "/lib");
  QResource::registerResource(main_app.get_asset("themes/" + Options::getInstance().theme() + ".rcc"));

  QFontDatabase fontDatabase;
  QDirIterator it(get_base_path() + "fonts",
                  QDirIterator::Subdirectories);
  while (it.hasNext())
    fontDatabase.addApplicationFont(it.next());

  const QList<QByteArray> image_formats = QImageReader::supportedImageFormats();
  if (!image_formats.contains("apng"))
    qCritical() << "QApng plugin could not be loaded";
  if (!image_formats.contains("webp"))
    qCritical() << "QWebp plugin could not be loaded";

  QString p_language =
      Options::getInstance().language();
  if (p_language == "  " || p_language == "")
    p_language = QLocale::system().name();

  QTranslator qtTranslator;
  qtTranslator.load("qt_" + p_language,
                    QLibraryInfo::location(QLibraryInfo::TranslationsPath));
  main_app.installTranslator(&qtTranslator);

  QTranslator appTranslator;
  qDebug() << ":/resource/translations/ao_" + p_language;
  appTranslator.load("ao_" + p_language, ":/resource/translations/");
  main_app.installTranslator(&appTranslator);

  if (UltaLock::enabled) {
    // First run: start on the default theme that ships with the client
    QSettings l_ulta(get_base_path() + "ulta.ini", QSettings::IniFormat);
    l_ulta.beginGroup("theme");
    if (!l_ulta.value("default_applied", false).toBool() &&
        QDir(get_ulta_builtin_path() + "themes/ULTADefault5x3").exists()) {
      Options::getInstance().setTheme("ULTADefault5x3");
      l_ulta.setValue("default_applied", true);
    }
    l_ulta.endGroup();
  }

  main_app.construct_lobby();
  main_app.net_manager->get_server_list(std::bind(&Lobby::list_servers, main_app.w_lobby));
  main_app.net_manager->send_heartbeat();
  main_app.w_lobby->show();
  return main_app.exec();
}
