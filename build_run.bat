@echo off
setlocal

rem ---------------------------------------------------------------
rem NOTE: this file MUST be saved as ANSI/GBK, NOT UTF-8.
rem cmd.exe parses .bat files using the system ANSI codepage and
rem ignores chcp, so UTF-8 Chinese here would break the script.
rem ---------------------------------------------------------------

chcp 936 >nul
cd /d "%~dp0"

set "GCC=C:\Dev-Cpp\bin\gcc.exe"

if not exist "%GCC%" (
    echo.
    echo [错误] 找不到编译器: %GCC%
    echo.
    pause
    exit /b 1
)

set "EXE=frame_parser.exe"

echo.
echo ==========================================
echo   编译全部 .c 文件
echo ==========================================

"%GCC%" *.c -o "%EXE%" -std=c99
if errorlevel 1 (
    echo.
    echo [失败] 上面的错误改掉之后，再运行一次本文件。
    echo.
    pause
    exit /b 1
)

echo.
echo [编译成功]  正在运行 %EXE%
echo ------------------------------------------

chcp 65001 >nul
"%EXE%"
chcp 936 >nul

echo ------------------------------------------
echo [程序已退出]
echo.
pause
