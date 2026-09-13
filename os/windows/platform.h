#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <winhttp.h>
#include <tlhelp32.h>
#include <shellapi.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winhttp.lib")

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>
#include <regex>
#include <algorithm>
#include <cctype>

namespace fs = std::filesystem;

inline bool isDiscordRunning(const std::wstring &targetExe = L"Discord.exe") {
  HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  if (hSnap == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W pe{};
  pe.dwSize = sizeof(pe);
  if (Process32FirstW(hSnap, &pe)) {
    do {
      if (_wcsicmp(pe.szExeFile, targetExe.c_str()) == 0) {
        CloseHandle(hSnap);
        return true;
      }
    } while (Process32NextW(hSnap, &pe));
  }
  CloseHandle(hSnap);
  return false;
}

inline void killDiscord(const std::wstring &targetExe = L"Discord.exe") {
  // targetExe is a fixed internal value. Invoke taskkill.exe directly;
  // never route process termination through cmd.exe / _wsystem.
  wchar_t systemRoot[MAX_PATH]{};
  DWORD n = GetEnvironmentVariableW(L"SystemRoot", systemRoot, MAX_PATH);
  std::wstring taskkill = (n > 0 && n < MAX_PATH)
      ? (std::wstring(systemRoot, n) + L"\\System32\\taskkill.exe")
      : L"C:\\Windows\\System32\\taskkill.exe";

  std::wstring cmdLine = L"\"" + taskkill + L"\" /F /T /IM \"" + targetExe + L"\"";
  STARTUPINFOW si{sizeof(si)};
  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(taskkill.c_str(), cmdLine.data(), nullptr, nullptr, FALSE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  if (ok) {
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
  }
  std::this_thread::sleep_for(std::chrono::seconds(1));
}

inline std::wstring findDiscordExeIn(const std::wstring &basePath) {
  std::wstring discordRoot = basePath + L"\\Discord";
  if (!fs::exists(discordRoot)) return L"";
  std::wstring latestPath, latestVersion;
  try {
    for (const auto &entry : fs::directory_iterator(discordRoot)) {
      if (!entry.is_directory()) continue;
      std::wstring folder = entry.path().filename().wstring();
      if (folder.rfind(L"app-", 0) != 0) continue;
      std::wstring exePath = (entry.path() / L"Discord.exe").wstring();
      if (fs::exists(exePath) && (latestVersion.empty() || folder > latestVersion)) {
        latestVersion = folder;
        latestPath = exePath;
      }
    }
  } catch (...) {
    return L"";
  }
  return latestPath;
}

inline std::wstring locateDiscordExe() {
  wchar_t pathBuf[MAX_PATH];
  if (GetEnvironmentVariableW(L"LOCALAPPDATA", pathBuf, MAX_PATH) != 0) {
    auto result = findDiscordExeIn(pathBuf);
    if (!result.empty()) return result;
  }
  if (GetEnvironmentVariableW(L"ProgramFiles", pathBuf, MAX_PATH) != 0) {
    auto result = findDiscordExeIn(pathBuf);
    if (!result.empty()) return result;
  }
  if (GetEnvironmentVariableW(L"ProgramFiles(x86)", pathBuf, MAX_PATH) != 0) {
    auto result = findDiscordExeIn(pathBuf);
    if (!result.empty()) return result;
  }
  return L"";
}

inline bool launchDiscord(const std::wstring &exePath, const std::string &proxyEndpoint) {
  // lpApplicationName is kept separate from the command line so the executable
  // path itself is never interpreted as shell syntax.
  std::wstring cmdLine;
  if (!proxyEndpoint.empty()) {
    std::wstring wProxy(proxyEndpoint.begin(), proxyEndpoint.end());
    cmdLine = L"--proxy-server=socks5://" + wProxy +
              L" --proxy-bypass-list=\"cdn.discordapp.com;*.discordapp.net;*.discord.media;<local>\"";
  }

  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE hNull = CreateFileW(L"NUL", GENERIC_WRITE | GENERIC_READ,
                             FILE_SHARE_WRITE | FILE_SHARE_READ, &sa,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (hNull == INVALID_HANDLE_VALUE) return false;

  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = hNull;
  si.hStdError = hNull;
  si.hStdInput = hNull;

  PROCESS_INFORMATION pi{};
  std::wstring workDir = fs::path(exePath).parent_path().wstring();
  BOOL ok = CreateProcessW(
      exePath.c_str(),
      cmdLine.empty() ? nullptr : cmdLine.data(),
      nullptr, nullptr, TRUE,
      CREATE_NO_WINDOW | DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP |
          CREATE_BREAKAWAY_FROM_JOB,
      nullptr, workDir.c_str(), &si, &pi);

  CloseHandle(hNull);
  if (!ok) return false;
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
  return true;
}

// Run a real executable directly. Unlike _popen(), this never invokes cmd.exe
// and therefore untrusted proxy text cannot become shell syntax.
inline std::string runProcessCapture(const std::wstring &exe,
                                     const std::vector<std::wstring> &args,
                                     DWORD timeoutMs = 15000) {
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  HANDLE readPipe = INVALID_HANDLE_VALUE, writePipe = INVALID_HANDLE_VALUE;
  if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return "";
  SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

  std::wstring cmd = L"\"" + exe + L"\"";
  for (const auto &arg : args) {
    cmd += L" \"";
    for (wchar_t c : arg) {
      if (c == L'\\') cmd += L'\\';
      if (c == L'\"') cmd += L'\\';
      cmd += c;
    }
    if (!arg.empty() && arg.back() == L'\\') cmd += L'\\';
    cmd += L"\"";
  }

  STARTUPINFOW si{sizeof(si)};
  si.dwFlags = STARTF_USESTDHANDLES;
  si.hStdOutput = writePipe;
  si.hStdError = writePipe;
  si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  PROCESS_INFORMATION pi{};

  BOOL ok = CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, TRUE,
                           CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
  CloseHandle(writePipe);
  if (!ok) {
    CloseHandle(readPipe);
    return "";
  }

  std::string result;
  DWORD start = GetTickCount();
  char buffer[4096];
  for (;;) {
    DWORD available = 0;
    if (!PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr)) break;
    if (available > 0) {
      DWORD got = 0;
      if (ReadFile(readPipe, buffer, (std::min<DWORD>)(available, sizeof(buffer)), &got, nullptr) && got)
        result.append(buffer, got);
    } else {
      DWORD wait = WaitForSingleObject(pi.hProcess, 50);
      if (wait == WAIT_OBJECT_0) {
        while (PeekNamedPipe(readPipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
          DWORD got = 0;
          if (!ReadFile(readPipe, buffer, (std::min<DWORD>)(available, sizeof(buffer)), &got, nullptr) || !got) break;
          result.append(buffer, got);
        }
        break;
      }
      if (GetTickCount() - start > timeoutMs) {
        TerminateProcess(pi.hProcess, 1);
        break;
      }
    }
  }
  CloseHandle(readPipe);
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
  return result;
}

inline std::wstring findCurl() {
  wchar_t path[MAX_PATH];
  DWORD n = SearchPathW(nullptr, L"curl.exe", nullptr, MAX_PATH, path, nullptr);
  return n > 0 && n < MAX_PATH ? std::wstring(path, n) : L"";
}

inline bool isValidProxyEndpoint(std::string proxy) {
  // Accept the formats emitted by ProxyScrape: socks5://host:port or host:port.
  constexpr const char *prefix = "socks5://";
  if (proxy.rfind(prefix, 0) == 0) proxy.erase(0, 9);
  if (proxy.empty() || proxy.find_first_of(" \t\r\n\"';&|<>`$") != std::string::npos)
    return false;

  std::string host;
  std::string port;
  if (!proxy.empty() && proxy.front() == '[') {
    auto close = proxy.find(']');
    if (close == std::string::npos || close + 2 > proxy.size() || proxy[close + 1] != ':') return false;
    host = proxy.substr(1, close - 1);
    port = proxy.substr(close + 2);
  } else {
    auto colon = proxy.rfind(':');
    if (colon == std::string::npos) return false;
    host = proxy.substr(0, colon);
    port = proxy.substr(colon + 1);
    if (host.find(':') != std::string::npos) return false; // IPv6 must be bracketed.
  }
  if (host.empty() || port.empty() || port.size() > 5 ||
      !std::all_of(port.begin(), port.end(), [](unsigned char c) { return std::isdigit(c); }))
    return false;
  unsigned long p = std::stoul(port);
  return p >= 1 && p <= 65535;
}

inline std::string normalizeProxy(std::string proxy) {
  if (proxy.rfind("socks5://", 0) == 0) proxy.erase(0, 9);
  return proxy;
}

inline std::string fetchProxies() {
  const std::wstring curl = findCurl();
  if (curl.empty()) return "";
  const std::wstring url =
    L"https://cdn.jsdelivr.net/gh/proxyscrape/free-proxy-list@main/proxies/protocols/socks5/data.txt";
  return runProcessCapture(curl, {L"-s", L"--max-time", L"10", url});
}

inline std::vector<std::string> fetchProxyList() {
  std::vector<std::string> proxies;
  std::string data = fetchProxies();
  std::istringstream stream(data);
  std::string line;
  while (std::getline(stream, line)) {
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    if (isValidProxyEndpoint(line)) proxies.push_back(normalizeProxy(line));
  }
  return proxies;
}

inline bool testProxy(const std::string &proxy) {
  if (!isValidProxyEndpoint(proxy)) return false;
  const std::wstring curl = findCurl();
  if (curl.empty()) return false;
  std::string normalized = normalizeProxy(proxy);
  std::wstring wProxy(normalized.begin(), normalized.end());
  std::string result = runProcessCapture(curl,
      {L"-s", L"--max-time", L"8", L"--proxy", L"socks5h://" + wProxy,
       L"https://discord.com/api/v10/gateway"}, 10000);
  return result.find("url") != std::string::npos || result.find("wss://") != std::string::npos;
}

inline std::string findWorkingProxy() {
  auto proxies = fetchProxyList();
  for (size_t i = 0; i < proxies.size() && i < 15; ++i) {
    if (testProxy(proxies[i])) return proxies[i];
  }
  return "";
}
