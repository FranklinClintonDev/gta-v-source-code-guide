@echo off

REM Get the drive letter where the .bat file is located without the colon
set driveletter=%~d0
set driveletter=%driveletter::=%

REM Get the full path to the folder containing the .bat file
set folderpath=%~dp0

REM Remove the trailing backslash from folder path
set folderpath=%folderpath:~0,-1%

echo ------------------------------------------------------
echo Please do only run this file from your build folder.
echo ------------------------------------------------------
pause

REM Create the network share using the drive letter and path
net use X: \\localhost\%driveletter%$\%folderpath:~3% /persistent:yes

echo Drive mapped successfully to X: using path \\localhost\%driveletter%$\%folderpath:~3%
pause
