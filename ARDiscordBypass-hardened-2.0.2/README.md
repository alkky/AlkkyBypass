# ARDiscordBypass — hardened Windows build 2.0.2

Esta é uma variante local endurecida do ARDiscordBypass original.

## O que foi corrigido

- Remove `_popen()` da busca/teste de proxies.
- Remove `_wsystem()` também: o encerramento do Discord chama `taskkill.exe` diretamente, sem shell.
- `curl.exe` é iniciado diretamente com `CreateProcessW`; o proxy nunca passa por `cmd.exe`/PowerShell.
- Proxy recebido da internet é validado antes de ser usado: `host:port` ou `socks5://host:port`, sem metacaracteres de shell e com porta 1–65535.
- `CreateProcessW` recebe o caminho do Discord em `lpApplicationName`, evitando interpretar o caminho como linha de comando.
- Se não houver `curl.exe` ou nenhum proxy funcionar, o Discord abre sem bypass em vez de executar um fallback PowerShell.
- O processo do bypass termina depois de iniciar o Discord.

## Build

Use um MinGW-w64/GCC no Windows e execute `build.bat`.

O projeto original recomenda C++20 + MinGW/GCC. Esta variante é somente Windows por enquanto.

## Importante

O repositório original declara uma licença que proíbe modificações sem autorização do autor. Portanto, trate isto como uma cópia local para análise/teste e não publique um fork modificado sem verificar a licença/permissão do autor.

Também não há garantia de que um bypass continue funcionando após mudanças no Discord ou mudanças regulatórias no Brasil.
