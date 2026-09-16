@echo off
setlocal
chcp 65001 >nul
cd /d "%~dp0"

set "GCC=C:\Dev-Cpp\bin\gcc.exe"

if not exist "%GCC%" (
    echo.
    echo [ERROR] 编译器没找到: %GCC%
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
    echo [FAILED] 把上面的错误改掉，再运行一次本文件。
    echo.
    pause
    exit /b 1
)

echo.
echo [Build OK]  运行 %EXE%
echo ------------------------------------------
"%EXE%"
echo ------------------------------------------
echo [Program exited]
echo.
pause
