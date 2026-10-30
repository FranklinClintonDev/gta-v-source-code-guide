@ECHO OFF
TITLE DurangoAdkKitPath Environment Variable Setup

SET "DurangoAdkKitPath=C:\Program Files (x86)\Microsoft Durango XDK\adk\"

ECHO Setting DurangoAdkKitPath...
ECHO %DurangoAdkKitPath%
ECHO.

REM Verify the CRT files MSBuild expects are present.
IF NOT EXIST "%DurangoAdkKitPath%crt\amd64\msvcr110.dll" GOTO :MISSING
IF NOT EXIST "%DurangoAdkKitPath%crt\amd64\msvcp110.dll" GOTO :MISSING
IF NOT EXIST "%DurangoAdkKitPath%crt\amd64\vccorlib110.dll" GOTO :MISSING

REM Save for future processes. Visual Studio/MSBuild must be restarted afterward.
SETX DurangoAdkKitPath "%DurangoAdkKitPath%" >NUL
IF ERRORLEVEL 1 GOTO :SETFAILED

ECHO ================================================
ECHO SUCCESS
ECHO ================================================
ECHO Saved DurangoAdkKitPath as:
ECHO %DurangoAdkKitPath%
ECHO.
ECHO Verified:
ECHO   %DurangoAdkKitPath%crt\amd64\msvcr110.dll
ECHO   %DurangoAdkKitPath%crt\amd64\msvcp110.dll
ECHO   %DurangoAdkKitPath%crt\amd64\vccorlib110.dll
ECHO.
ECHO Close Visual Studio completely, then reopen it before rebuilding.
ECHO.
PAUSE
EXIT /B 0

:MISSING
ECHO ================================================
ECHO ERROR: Required CRT file(s) not found.
ECHO ================================================
ECHO Expected folder:
ECHO %DurangoAdkKitPath%crt\amd64\
ECHO.
ECHO Make sure these files exist there:
ECHO   msvcr110.dll
ECHO   msvcp110.dll
ECHO   vccorlib110.dll
ECHO.
PAUSE
EXIT /B 1

:SETFAILED
ECHO ================================================
ECHO ERROR: Failed to save DurangoAdkKitPath.
ECHO ================================================
ECHO.
PAUSE
EXIT /B 1
