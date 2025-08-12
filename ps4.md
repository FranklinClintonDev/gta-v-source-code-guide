# WIP
# Grand Theft Auto V: Source Code Build Guide *For PS4*

## Requirements:
- [VS 2017](https://archive.org/details/VisualStudio2017RTMISOX8664)
- [GTA V Source Code Toolset](https://pixeldrain.com/u/2XdabBq3)
- [GTA V for PS4](https://1fichier.com/?owilwri8303p58o3x72u&af=3662447%20) 
    - [Update RPF to v1.46](https://1fichier.com/?xdiqoe5n3c6mqgixa0ns&af=3662447)
- GTAVSP.7z - Source Code  
   - SHA1 Hash: `ca39323730ed644fa534a2946506d4287f92a799`
   - Password: `Mi76#b>9mRed`
- A Jailbroken PS4 is required for running the build.
   - You can find tutorials by modded warfare on how to do it. 
   - Before you even try this make sure your PS4 Firmware is 9:00-11:00, maybe even 12:00

## Installing the Toolset

**You should deactivate your Anti-virus protection before doing any of the next steps.**

1. Run `Install_Toolset.bat`

#### Visual Studio 2012

   - At "Optional features to install": only select "Microsoft Foundation Classes for C++"

#### IncrediBuild

   - At "Welcome" select "Install IncrediBuild"
   - At "Component Selection" select IncrediBuild Agent and IncrediBuild Coordinator
   - Do not change any other settings

#### .Net Framework 3.5

   - If the installer freezes:
     - Go to `Installation_Files\3. .Net Framework 3.5 Installer\Source Files`
     - Copy `setup.bat` to `3. .Net Framework 3.5 Installer` and run it as administrator

#### DirectX SDK (June 2010)

   - If you get error S1023:
     - Uninstall Visual C++ 2010 Redistributable
     - Reinstall DirectX SDK (June 2010)
   - At "Help Improve the DirectX SDK" select "No, I would not like to participate"

#### Helix-Core-Server

   - Don't change the port
   - For path, just select any empty folder

## Creating X:\ Drive

1. Create a new folder called "GTA" on the Desktop or anywhere you want  
2. Inside the "GTA" folder, create a folder called "gta5"  
3. Copy all content from "GTAV Source" folder to "gta5"  
4. Copy the two bat files from the [`GTA V - X Drive Batch files`](GTA%20V%20-%20X%20Drive%20Batch%20files) folder to your "GTA" folder  
5. Run `Connect_X_Drive.bat`  
6. Right-click on the gta5 folder, turn off the "Read-Only" option, then press "Apply"
