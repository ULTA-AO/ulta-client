#ifndef FILE_FUNCTIONS_H
#define FILE_FUNCTIONS_H

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QCoreApplication>

bool file_exists(QString file_path);
bool dir_exists(QString file_path);
bool exists(QString p_path);
QString get_base_path();
// ULTA: folder with content bundled with the client (e.g. the effects pack).
// It is searched LAST, after the player's own base and mounted folders.
QString get_ulta_builtin_path();

#endif // FILE_FUNCTIONS_H
