THIS WAS MADE WITH AI SO I MIGHT CONTAIN FALSE INFO
### GTA V PS4 Boot Arguments

#### Core Game Arguments
- -override_script=script: Overrides the default script.
- -altGameConfig=gameconfig2699.xml: Specifies an alternative game configuration file.
- -nompsavegame: Disables saving the game in multiplayer mode.
- -DoReleaseStartup: Executes release startup sequence.
- -rootdir=/app0/: Specifies the root directory.
- -update=/app0/update/: Specifies the update directory.
- -commonpack=/app0/common.rpf: Loads the common resources pack.
- -platformpack=/app0/ps4.rpf: Loads the platform-specific resources pack.

#### Networking Arguments
- -net_all=debug3: Sets all networking to debug level 3.
- -rline_all=debug3: Configures Rockstar Online debugging to level 3.
- -netSessionIgnoreDataHash: Ignores data hash mismatches in network sessions.
- -netSessionIgnoreCheater: Ignores cheater flags in network sessions.
- -netSessionIgnoreAim: Ignores aim-related data in network sessions.
- -netSessionIgnoreTimeout: Ignores timeouts in network sessions.
- -netSessionIgnoreVersion: Ignores version mismatches in network sessions.

#### Debugging and Development
- -npForceAllowPermissions: Forces permission checks to pass.
- -rlNpPlusAuthorised: Rockstar Network Plus authorization override.
- -sc_noPrologueCheckForMP: Skips the prologue check for multiplayer access.
- -nethttpdump: Dumps HTTP network traffic for debugging purposes.
- -output: Enables output logging or debug output.

#### Platform-Specific Commands
- -forceboothdd: Forces the game to boot from HDD.
- -ttyframeprefix: Adds a TTY frame prefix to debug logs.
- -ragAddr=<IP>: Specifies a RAGE (Rockstar Advanced Game Engine) server address.
- -ragUseOwnWindow: Forces the engine to use a separate window.

#### Commented or Possibly Optional
- // -useFinalShaders: May enable or disable the use of final shaders in development builds.
- // -rag: A placeholder or test argument for the RAGE engine.
- // -output: Might duplicate the earlier -output command.
