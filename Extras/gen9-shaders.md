# How to compile *PS5/Xbox Series* X (Gen 9) Shaders.

#### ***This is experimental and not fully working***

### Requirements:
 - A [prebuilt version of normal shaders](https://pixeldrain.com/u/vAvUVD8B) or you can compile them.
 - Make sure you setup the source code (follow all steps on PC Guide up to [Patching Source Code and Tools](../PC.md#patching-source-code-and-tools))

### Configuration Settings

1. Run `X:\gta5\src\dev_ng\game\VS_Project\load_sln_unity_2012.bat`.

<div style="text-align:center;">
<img src="../IMG/01.png" alt="image" style="width:30%;">
</div>

- A warning will appear in the Command Prompt Window stating you are missing an SDK, please ignore it and press any key to continue and open it with Visual Studio 2012.

<div style="text-align:center;">
<img src="../IMG/02.png" alt="image" style="width:50%;">
</div>

- If you are prompted with `Choose Default Environment Settings` Select `General Development Settings` and at `Local Help Documentation` select `None`.

<div style="text-align:center;">
<img src="../IMG/03.png" alt="image" style="width:50%;">
</div>

2. Once the solution loads, open the drop-down menu that says `Debug` at the top, select `Configuration Manager`.

<div style="text-align:center;">
<img src="../IMG/04.png" alt="image" style="width:50%;">
</div>

3. Change `Active Solution Platform` to `x64` then close the configuration window.

<div style="text-align:center;">
<img src="../IMG/16.png" alt="image" style="width:50%;">
</div>

4. In the `solution explorer` scroll down to `shaders_rc` and right click then click on properties.

<div style="text-align:center;">
<img src="../IMG/17.png" alt="image" style="width:30%;">
</div>

5. Set the `Platform` to `Win32` and the `Configuration` to `(Debug Win32 4.0)`.

<div style="text-align:center;">
<img src="../IMG/18.png" alt="image" style="width:50%;">
</div>

6. Click on `NMake` then set:

Build Command Line to `$(MSBuildProjectDirectory)\batch\rsm_build_win32_50.bat dev`

Rebuild All Command Line to `$(MSBuildProjectDirectory)\batch\rsm_rebuild_win32_50.bat dev`

Clean Command Line to `$(MSBuildProjectDirectory)\batch\rsm_clean_win32_50.bat dev`

<div style="text-align:center;">
<img src="../IMG/19.png" alt="image" style="width:50%;">
</div>

### Compiling the Shaders

1. On the `solution explorer` scroll down to `shaders_rc` and right click then click on `build`.

To see the progress and watch its status, in the taskbar, open the Overflow Menu (the small ^ in the taskbar on the right).
Find `IncrediBuild Agent` (the one with a green arrow), right-click it, and press `Build Monitor`.
In `Build Monitor`, on the left press the icon at the top that should say `Progress`.
You should now see what you're building, and at the bottom left of the window, you'll see a percentage of how complete it is out of 100%.

2. Wait for the build to finish (Expect errors).
3. The build will fail but most of it should have compiled.
4. Your compiled shaders should be in `X:\gta5\titleupdate\dev_ng\common\shaders\win32_50`

Make sure you got a normal shader copy compiled 
 - You can get prebuild shaders [here](https://pixeldrain.com/u/vAvUVD8B).
 - Or compile them yourself with [this guide](../PC.md#building-shaders). 
    - Make sure to restore the default settings before using this guide.

5. Drag the contents from `X:\gta5\titleupdate\dev_ng\common\shaders\win32_50` to the `win32_40` folder of your normal shader copy.
6. Drop the `win32_40` folder in you game directory in this path `Grand Theft Auto V\common\shaders\win32_40`.

To use the shaders [run a compiled build](../PC.md#running-the-game).
