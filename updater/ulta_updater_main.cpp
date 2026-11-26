// ULTA AO updater: installs a downloaded release over the client's folder.
//
//   ulta-updater --zip <release.zip> --dir <client folder> --pid <client pid> --exe <client exe name>
//
// Waits for the client to close, unpacks the zip into update/staging, moves
// replaced files to update/backup and the new ones in. Puts the old files
// back on failure, then starts the client again.

#include "miniz.h"

#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::wofstream g_log;

static void log(const std::wstring &line)
{
  if (g_log.is_open()) {
    g_log << line << L"\n";
    g_log.flush();
  }
}

// A name inside the zip is only accepted if it stays inside the client folder.
static bool safe_relative(const std::string &name)
{
  if (name.empty() || name[0] == '/' || name[0] == '\\' || name.find(':') != std::string::npos)
    return false;
  size_t start = 0;
  while (start <= name.size()) {
    size_t end = name.find_first_of("/\\", start);
    if (end == std::string::npos)
      end = name.size();
    const std::string part = name.substr(start, end - start);
    if (part == "..")
      return false;
    start = end + 1;
  }
  return true;
}

static bool protected_path(const std::string &rel)
{
  static const char *roots[] = {"base/", "logs/", "update/", "ulta.ini", "config.ini"};
  for (const char *root : roots) {
    if (rel.rfind(root, 0) == 0)
      return true;
  }
  return false;
}

static bool move_with_retry(const fs::path &from, const fs::path &to)
{
  for (int attempt = 0; attempt < 40; ++attempt) {
    std::error_code ec;
    fs::remove(to, ec);
    ec.clear();
    fs::rename(from, to, ec);
    if (!ec)
      return true;
    Sleep(250); // an antivirus scan or a lingering lock
  }
  return false;
}

static void start_client(const fs::path &dir, const std::wstring &exe)
{
  const fs::path path = dir / exe;
  STARTUPINFOW si{};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi{};
  std::wstring cmd = L"\"" + path.wstring() + L"\"";
  if (CreateProcessW(path.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.c_str(), &si, &pi)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
  else {
    log(L"could not start the client");
  }
}

int wmain(int argc, wchar_t **argv)
{
  std::map<std::wstring, std::wstring> args;
  for (int i = 1; i + 1 < argc; i += 2)
    args[argv[i]] = argv[i + 1];
  if (!args.count(L"--zip") || !args.count(L"--dir") || !args.count(L"--pid") || !args.count(L"--exe"))
    return 2;
  const fs::path zip_path = args[L"--zip"];
  const fs::path dir = args[L"--dir"];
  const std::wstring exe = args[L"--exe"];
  const DWORD pid = static_cast<DWORD>(std::stoul(args[L"--pid"]));

  std::error_code ec;
  fs::create_directories(dir / "update", ec);
  g_log.open(dir / "update" / "update.log", std::ios::trunc);
  log(L"update started: " + zip_path.wstring());

  // 1. Wait for the client to close.
  if (HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, pid)) {
    WaitForSingleObject(process, 60000);
    CloseHandle(process);
  }

  const fs::path staging = dir / "update" / "staging";
  const fs::path backup = dir / "update" / "backup";
  std::vector<std::pair<fs::path, fs::path>> replaced; // target, backup (empty if it was new)
  bool ok = false;
  try {
    // 2. Read the zip.
    FILE *file = _wfopen(zip_path.c_str(), L"rb");
    if (!file)
      throw std::runtime_error("cannot open the zip");
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_cfile(&zip, file, 0, 0)) {
      fclose(file);
      throw std::runtime_error("not a valid zip");
    }
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    std::vector<std::string> names;
    for (mz_uint i = 0; i < count; ++i) {
      if (mz_zip_reader_is_file_a_directory(&zip, i))
        continue;
      char name[1024];
      mz_zip_reader_get_filename(&zip, i, name, sizeof(name));
      if (!safe_relative(name)) {
        mz_zip_reader_end(&zip);
        fclose(file);
        throw std::runtime_error(std::string("unsafe path in the zip: ") + name);
      }
      names.push_back(name);
    }
    if (names.empty())
      throw std::runtime_error("the zip is empty");
    // A release zip has one top-level folder: drop it.
    std::string prefix;
    const size_t slash = names[0].find('/');
    if (slash != std::string::npos) {
      prefix = names[0].substr(0, slash + 1);
      for (const std::string &n : names)
        if (n.rfind(prefix, 0) != 0)
          prefix.clear();
    }

    // 3. Unpack into staging.
    fs::remove_all(staging, ec);
    fs::create_directories(staging);
    std::vector<std::string> rels;
    for (mz_uint i = 0; i < count; ++i) {
      if (mz_zip_reader_is_file_a_directory(&zip, i))
        continue;
      char name[1024];
      mz_zip_reader_get_filename(&zip, i, name, sizeof(name));
      std::string rel = std::string(name).substr(prefix.size());
      if (rel.empty() || protected_path(rel))
        continue;
      size_t size = 0;
      void *data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
      if (!data)
        throw std::runtime_error("could not unpack " + rel);
      const fs::path out = staging / fs::u8path(rel);
      fs::create_directories(out.parent_path());
      std::ofstream stream(out, std::ios::binary);
      stream.write(static_cast<const char *>(data), static_cast<std::streamsize>(size));
      stream.close();
      mz_free(data);
      if (!stream)
        throw std::runtime_error("could not write " + rel);
      rels.push_back(rel);
    }
    mz_zip_reader_end(&zip);
    fclose(file);
    log(L"unpacked " + std::to_wstring(rels.size()) + L" files");

    // 4. Move the old files to backup, the new files in.
    fs::remove_all(backup, ec);
    fs::create_directories(backup);
    for (const std::string &rel : rels) {
      const fs::path target = dir / fs::u8path(rel);
      const fs::path saved = backup / fs::u8path(rel);
      fs::create_directories(target.parent_path());
      if (fs::exists(target)) {
        fs::create_directories(saved.parent_path());
        if (!move_with_retry(target, saved))
          throw std::runtime_error("cannot replace " + rel);
        replaced.emplace_back(target, saved);
      }
      else {
        replaced.emplace_back(target, fs::path());
      }
      if (!move_with_retry(staging / fs::u8path(rel), target))
        throw std::runtime_error("cannot install " + rel);
    }
    ok = true;
  }
  catch (const std::exception &e) {
    log(L"FAILED: " + std::wstring(e.what(), e.what() + strlen(e.what())));
  }

  if (!ok) {
    // Put everything back.
    for (auto it = replaced.rbegin(); it != replaced.rend(); ++it) {
      fs::remove(it->first, ec);
      if (!it->second.empty())
        move_with_retry(it->second, it->first);
    }
    log(L"rolled back");
  }
  else {
    fs::remove_all(staging, ec);
    fs::remove(zip_path, ec);
    log(L"update installed");
  }
  start_client(dir, exe);
  return ok ? 0 : 1;
}
