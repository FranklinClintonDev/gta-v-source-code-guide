@echo off
echo -------------------------------------------------------------------------------------------
echo This will install the required environment variables for PS4 SDK versions 1.700 and 2.500
echo -------------------------------------------------------------------------------------------
pause 

@echo on

REM Set system environment variables for PATH
setx PATH "%PATH%;C:\Program Files (x86)\SCE\ORBIS SDKs\2.500\host_tools\bin" /M

REM Set additional system environment variables
setx SCE_ORBIS_SAMPLE_DIR "C:\Program Files (x86)\SCE\ORBIS SDKs\1.700\host_tools\samples" /M
setx SCE_ORBIS_SDK_DIR "C:\Program Files (x86)\SCE\ORBIS SDKs\1.700" /M
setx SCE_ROOT_DIR "C:\Program Files (x86)\SCE" /M

@echo off
echo Environment variables have been set successfully.
pause