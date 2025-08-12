@echo off
REM Disconnect the X: drive
net use X: /delete

REM Check if the disconnection was successful
if %errorlevel% equ 0 (
    echo Drive X: has been successfully ejected.
) else (
    echo Failed to eject drive X:. It may not be connected.
)

pause
