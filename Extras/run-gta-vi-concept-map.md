# How to boot the GTA VI concept map from June 2014 inside GTA V

## Requirements
- You just need the full "P1" folder from the leaked Torrent.

*This guide does not distribute or link to any leaked copy of the source code; you will need to obtain it yourself.*

## Creating X:\ Drive
1. Copy the two bat files from the [`GTA V - X Drive Batch files`](../GTA%20V%20-%20X%20Drive%20Batch%20Files) folder to your `P1` folder  
2. Run `Connect_X_Drive.bat`
3. Put [`this file`](../GTA%20V%20-%20Source%20Code%20Patches/PC/All_In_One_Install_Environment_Variables.bat) inside `X:\gta5\src\dev_ng\`
4. Run `X:\gta5\src\dev_ng\All_In_One_Install_Environment_Variables.bat`  
5. Run `X:\gta5\src\dev_ng\setenv.bat`

## Patching Tools
-  Copy all folders from [`dll_patches_x.zip`](../GTA%20V%20-%20Source%20Code%20Patches/tools%20patch) to `X:\gta5\tools_ng\bin`, ensuring you overwrite existing files.

*This is optional but i recommend it you can patch the rag to make it work or remove it from the startup.*

## Patch env_test
- Replace `images.meta` inside `X:\gta5\build\dev_ng\common\data\levels\env_test` with [`this one`](Run%20GTA%20VI%20Concept%20Map/images.meta).

## Fix boot arguments
- edit `X:\gta5\build\dev_ng\game_win64_bankrelease.bat` and add -level env_test to set ARGUMENTS
Example:

```batch
set ARGUMENTS=-rootdir=%ROOTDIR% -level env_test -noautoload -logfile=%cd%/console_PC.log -snet_tty=debug3 -ragenet_tty=debug1 %updateparam% 
```
- paste [`this file`](Run%20GTA%20VI%20Concept%20Map/args.txt) inside `X:\gta5\build\dev_ng\`.

## Booting the Game

To boot the game just launch `X:\gta5\build\dev_ng\game_win64_bankrelease.bat`.
 - It might take long to load in.
 Once your inside the game use the tab key to change to game keyboard.
  - The Debug keys seem to be a bit diffrent on this build as it is from 2014.

# THE GAME TENTS TO CRASH ON THIS MAP.

# IM NOT AN EXPERT THATS JUST HOW I BOOTED IT IF YOU HAVE ISSUES OR ADDITIONS ADD THEM AT ISSUES OR PULL REQUESTS OR ON OUR DISCORD SERVER.

