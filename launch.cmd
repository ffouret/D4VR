@echo off
setlocal
where py >nul 2>nul
if errorlevel 1 (
    set "TASK_PYTHON=python"
) else (
    set "TASK_PYTHON=py -3"
)
if "%~1"=="" (
    %TASK_PYTHON% "%~dp0tools\d4vr.py" run
) else (
    %TASK_PYTHON% "%~dp0tools\d4vr.py" %*
)
set "TASK_RESULT=%errorlevel%"
if not "%TASK_RESULT%"=="0" pause
exit /b %TASK_RESULT%
