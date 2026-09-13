#ifdef _WIN32
#include "os/windows/platform.h"
#else
#error "esse bagulho é pra win 11/10."
#endif

#include <regex>
#include <iostream>
#include <string>

#define APP_VERSION "2.0.0"
#define GITHUB_REPO "alkky/AlkkyBypass"

void logf(const std::string &msg) { std::cout << "ARDCB - " << msg << std::endl; }

static std::string fetchLatestVersion() {
  HINTERNET hSession = WinHttpOpen(L"ARDCB/2.0.0",
                                   WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                   WINHTTP_NO_PROXY_NAME,
                                   WINHTTP_NO_PROXY_BYPASS, 0);
  if (!hSession) return "";
  HINTERNET hConnect = WinHttpConnect(hSession, L"api.github.com",
                                      INTERNET_DEFAULT_HTTPS_PORT, 0);
  if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
  HINTERNET hRequest = WinHttpOpenRequest(
      hConnect, L"GET", L"/repos/" GITHUB_REPO "/releases/latest",
      nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
      WINHTTP_FLAG_SECURE);
  if (!hRequest) {
    WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return "";
  }
  std::string response;
  if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
      WinHttpReceiveResponse(hRequest, nullptr)) {
    DWORD bytesRead = 0;
    char buffer[4096];
    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead) {
      response.append(buffer, bytesRead);
      bytesRead = 0;
    }
  }
  WinHttpCloseHandle(hRequest);
  WinHttpCloseHandle(hConnect);
  WinHttpCloseHandle(hSession);

  std::smatch match;
  if (std::regex_search(response, match,
                        std::regex("\\\"tag_name\\\"\\s*:\\s*\\\"([^\\\"]+)\\\""))) {
    std::string tag = match[1].str();
    if (!tag.empty() && tag[0] == 'v') tag.erase(0, 1);
    return tag;
  }
  return "";
}

static void checkForUpdate() {
  logf("Verificando atualizações...");
  const std::string latest = fetchLatestVersion();
  if (latest.empty()) { logf("num foi possível verificar atualizações."); return; }
  if (latest != APP_VERSION) {
    logf("tem uma versão diferente disponível no git: " + latest);
  } else {
    logf("cê tá na versão " APP_VERSION ".");
  }
}

int main(int argc, char *argv[]) {
  SetConsoleOutputCP(CP_UTF8);
  std::cout << "AlkPass" APP_VERSION << "\n";
  std::cout << "lol. olhe, minha camera funciona denovo!\n\n";

  WSADATA wsaData{};
  WSAStartup(MAKEWORD(2, 2), &wsaData);
  checkForUpdate();

  std::wstring discordExe = locateDiscordExe();
  if (discordExe.empty() && argc > 1) {
    int wlen = MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, nullptr, 0);
    if (wlen > 0) {
      std::wstring arg(wlen, L'\0');
      MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, arg.data(), wlen);
      arg.resize(wlen - 1);
      discordExe = arg;
    }
  }
  if (discordExe.empty()) {
    logf("ERRO: não encontrei a instalação do discord.");
    WSACleanup();
    return 1;
  }

  logf("discord encontrado: " + std::string(discordExe.begin(), discordExe.end()));
  if (isDiscordRunning()) {
    logf("discord aberto detectado; encerrando para aplicar os argumentos.");
    killDiscord();
  }

  logf("Buscando um proxy SOCKS5...");
  const std::string selectedProxy = findWorkingProxy();
  if (selectedProxy.empty()) {
    logf("nenhum proxy validado respondeu; abrindo o discord normalmente.");
  } else {
    logf("proxy validado selecionado.");
  }

    if (!launchDiscord(discordExe, selectedProxy)) {
    logf("ERRO: não foi possível iniciar o discord.");
    WSACleanup();
    return 1;
  }

  logf("discord iniciado.");
  WSACleanup();

  std::cout << "\npressione ENTER para sair...\n";
  std::cin.get();

  return 0;
}
