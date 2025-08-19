@ECHO OFF
ECHO Setting Up Environment Variables: This file includes two different setups.
ECHO Ensure the X drive is properly configured before running this script.
ECHO You will need to run both setups.

PAUSE
SETLOCAL ENABLEDELAYEDEXPANSION

REM Check if the paths exist
IF NOT EXIST "X:\gta5\tools_ng" (
    ECHO Error: Base directory X:\gta5\tools_ng does not exist!
    PAUSE
    EXIT /B 1
)

REM Set environment variables
SET POWERSHELL=powershell.exe
SET RS_PREV_TOOLSROOT=X:\gta5\tools_ng
SET RS_TOOLSROOT=X:\gta5\tools_ng
SET RS_CHECKFILE=X:\gta5\tools_ng\config.xml
SET RS_INSTALLER=X:\gta5\tools_ng\bin\install\installer\Installer.exe
SET TOOLSBIN=X:\gta5\tools_ng\bin
SET TOOLSLIB=X:\gta5\tools_ng\lib
SET REGISTER_EXTENSIONS_SCRIPT=X:\gta5\tools_ng\lib\util\registry\generate_register_extensions.rb
SET REGISTER_EXTENSIONS=X:\gta5\tools_ng\tmp\register_extensions.reg
SET DXSDK_DIR="C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)"
SET RS_CODEBRANCH=X:\gta5\src\dev_ng
SET RS_PROJECT=gta5
SET RAGE_DIR=X:\gta5\src\dev_ng\rage
SET RS_BUILDBRANCH=X:\gta5\build\dev_ng
SET RS_PROJROOT=X:\gta5
SET RS_SCRIPTBRANCH=X:\gta5\script\dev_ng
SET RUBYLIB=X:\gta5\tools_ng\lib
SET RUBYOPT=rubygems

REM Debugging output
ECHO POWERSHELL=%POWERSHELL%
ECHO RS_PREV_TOOLSROOT=%RS_PREV_TOOLSROOT%
ECHO RS_TOOLSROOT=%RS_TOOLSROOT%
ECHO RS_CHECKFILE=%RS_CHECKFILE%
ECHO RS_INSTALLER=%RS_INSTALLER%
ECHO TOOLSBIN=%TOOLSBIN%
ECHO TOOLSLIB=%TOOLSLIB%
ECHO REGISTER_EXTENSIONS_SCRIPT=%REGISTER_EXTENSIONS_SCRIPT%
ECHO REGISTER_EXTENSIONS=%REGISTER_EXTENSIONS%
ECHO DXSDK_DIR=%DXSDK_DIR%
ECHO RS_CODEBRANCH=%RS_CODEBRANCH%
ECHO RS_PROJECT=%RS_PROJECT%
ECHO RAGE_DIR=%RAGE_DIR%
ECHO RS_BUILDBRANCH=%RS_BUILDBRANCH%
ECHO RS_PROJROOT=%RS_PROJROOT%
ECHO RS_SCRIPTBRANCH=%RS_SCRIPTBRANCH%
ECHO RUBYLIB=%RUBYLIB%
ECHO RUBYOPT=%RUBYOPT%

REM Set permanent environment variables using setx
SETX POWERSHELL %POWERSHELL%
SETX RS_PREV_TOOLSROOT %RS_PREV_TOOLSROOT%
SETX RS_TOOLSROOT %RS_TOOLSROOT%
SETX RS_CHECKFILE %RS_CHECKFILE%
SETX RS_INSTALLER %RS_INSTALLER%
SETX TOOLSBIN %TOOLSBIN%
SETX TOOLSLIB %TOOLSLIB%
SETX REGISTER_EXTENSIONS_SCRIPT %REGISTER_EXTENSIONS_SCRIPT%
SETX REGISTER_EXTENSIONS %REGISTER_EXTENSIONS%
SETX DXSDK_DIR %DXSDK_DIR%
SETX RS_CODEBRANCH %RS_CODEBRANCH%
SETX RS_PROJECT %RS_PROJECT%
SETX RAGE_DIR %RAGE_DIR%
SETX RS_BUILDBRANCH %RS_BUILDBRANCH%
SETX RS_PROJROOT %RS_PROJROOT%
SETX RS_SCRIPTBRANCH %RS_SCRIPTBRANCH%
SETX RUBYLIB %RUBYLIB%
SETX RUBYOPT %RUBYOPT%

@echo off
ECHO Setup 1 completed successfully!
ECHO Proceeding to Setup 2...
pause 


REM Main variables, like root directories.
setx RS_BUILDBRANCH x:\gta5\build\dev_ng
setx RS_CODEBRANCH x:\gta5\src\dev_ng
setx RS_PROJECT gta5
setx RS_PROJROOT x:\gta5
setx RS_SCRIPTBRANCH x:\gta5\script\dev_ng
setx RS_TOOLSROOT x:\gta5\tools_ng
setx RAGE_DIR x:\gta5\src\dev_ng\rage
setx RS_TITLE_UPDATE_NG x:\gta5\titleupdate\dev_ng

REM Random variables required by various build systems and tools
setx RS_BUILDROOT x:\gta5\build
setx RS_ASSETS x:\gta5\assets_ng
setx RS_EXPORT x:\gta5\assets_ng\export
setx RS_PROCESSED x:\gta5\assets_ng\processed
setx RS_ASSETS_TU x:\gta5\assets_ng\titleupdate

setx RS_TOOLSCONFIG x:\gta5\tools_ng\etc
setx RS_TOOLSBIN x:\gta5\tools_ng\bin
setx RS_TOOLSDCC x:\gta5\tools_ng\dcc
setx RS_TOOLSLIB x:\gta5\tools_ng\lib
setx RS_TOOLSIRONLIB x:\gta5\tools_ng\ironlib
setx RS_TOOLSSCRIPT x:\gta5\tools_ng\script
setx RS_TOOLSSRC x:\gta5\src\dev_ng\rage\framework\tools\src
setx RS_TOOLSRUBY x:\gta5\tools_ng\bin\ruby\bin\ruby.exe
setx RS_TOOLSIR x:\gta5\tools_ng\bin\ironruby\bin\ir64.exe
setx RS_TOOLSPYTHON x:\gta5\tools_ng\bin\python\App\python.exe
setx RAGE_3RDPARTY X:\3rdparty\dev
setx RAGE_CRASHDUMP_DIR x:\gta5\build\dev_ng
setx RS_TITLE_UPDATE x:\gta5\titleupdate\dev
setx RUBYOPT rubygems
setx RUBYLIB x:\gta5\tools_ng\lib

setx RS_TOOLSCONVERT x:\gta5\tools_ng\ironlib\lib\RSG.Pipeline.Convert.exe

ECHO VARIABLES CONFIGURED:
ECHO RS_BUILDBRANCH:    %RS_BUILDBRANCH%
ECHO RS_CODEBRANCH:     %RS_CODEBRANCH%
ECHO RS_PROJECT:        %RS_PROJECT%
ECHO RS_PROJROOT:       %RS_PROJROOT%
ECHO RS_SCRIPTBRANCH:   %RS_SCRIPTBRANCH%
ECHO RS_TOOLSROOT:      %RS_TOOLSROOT%
ECHO RAGE_DIR:          %RAGE_DIR%
ECHO RS_TITLE_UPDATE_NG: %RS_TITLE_UPDATE_NG%

CALL %RS_TOOLSROOT%\bin\setenv.bat

ECHO Environment variables have been set.

PAUSE
