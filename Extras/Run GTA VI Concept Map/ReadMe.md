# How to boot the GTA VI concept map from June 2014 inside GTA V

## Requirements
- You just need the full "P1" folder from the leaked torrent.
- Make sure to allways create backups before editing anything.

*This guide does not distribute or link to any leaked copy of the source code or assets; you will need to obtain it yourself.*

## Creating X:\ Drive
1. Copy the two `.bat` files from the [`GTA V - X Drive Batch files`](../../GTA%20V%20-%20X%20Drive%20Batch%20Files) folder to your `P1` folder  
2. Run `Connect_X_Drive.bat`
3. Put [`this file`](../../GTA%20V%20-%20Source%20Code%20Patches/PC/All_In_One_Install_Environment_Variables.bat) inside `X:\gta5\src\dev_ng\`
4. Run `X:\gta5\src\dev_ng\All_In_One_Install_Environment_Variables.bat`  
5. Run `X:\gta5\src\dev_ng\setenv.bat`

## Patching tools
-  Copy all folders from [`dll_patches_x.zip`](../../GTA%20V%20-%20Source%20Code%20Patches/tools%20patch) to `X:\gta5\tools_ng\bin`, making sure to overwrite any existing files.

*This is optional, but I recommend it. You can patch RAG to make it work, or remove it from startup.*

## Patching the build
- Add [`this file`](args.txt) to `X:\gta5\build\dev_ng\`.
    - This adds the required boot arguments. You can remove `-rag` and `-raguseOwnWindow` if you don't want to use the RAG.
- Add [`this file`](gameconfig.xml) to `X:\gta5\build\dev_ng\common\data`
    - This prevents the game from crashing because of a dummy object.
- Replace `images.meta` in `X:\gta5\build\dev_ng\common\data\levels\env_test` with [`this one`](images.meta).
    - This is required for the game to boot you in the correct map.
- Replace `playerinfo.ymt` in `X:\gta5\build\dev_ng\x64\data\tune` with [`this one`](playerinfo.ymt)
    - This prevents the game from killing you when you move too far away.
- To install the crash hotfix extract both `.exe` files from [`this archive`](crash_hotfix.zip) to `X:\gta5\build\dev_ng\`.
    - Then run `GTA_RAGE_Crash_Hotfix_NoBackup.exe`. If you want to revert the changes, run `GTA_RAGE_Crash_Hotfix_Revert.exe` you can delete the `.exe` files after its installed.
    - If you revert the changes, it will also remove `-nocloth` from `X:\gta5\build\dev_ng\args.txt`.
    
## Booting the game

To boot the game, just launch `X:\gta5\build\dev_ng\game_win64_bankrelease.bat`.
 - It might take a long time to load.
 - Once you're inside the game, press the Tab key to switch keyboard input to game.
  - The debug keys seem to be a bit different on this build, as it is from 2014.

## Optional fixes
- You can add all files from [`this folder`](MapViewer%20Fix) to `X:\gta5\tools_ng\bin\MapViewer\Data` to fix the map viewer and being able to teleport.
  - The map is a fan-made rendering of the real concept map.

## The game might still crash frequently, as this map is very unstable. If you find any problems with the guide, please let me know and report the issue. The same goes for any additional information you would like to add.
