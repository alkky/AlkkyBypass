@echo off
setlocal
where g++ >nul 2>&1
if errorlevel 1 (
  echo GCC/MinGW nao encontrado no PATH.
  echo Instale um MinGW-w64 ou use o toolchain que voce ja usa para o projeto.
  exit /b 1
)
g++ -O2 -std=c++20 -static main.cpp -lws2_32 -lwinhttp -lshell32 -o AlkPass.exe
if errorlevel 1 exit /b 1
echo.
echo Build concluido: AlkPass.exe
