# bo3-audio-fix

A one-file fix for **Call of Duty: Black Ops III** starting with sound but a black or grey, "Not Responding" screen on PCs with lots of audio devices (Voicemeeter, VB-Cable, Voicemod, Steam Streaming, NVIDIA Broadcast and so on).

The usual advice is to disable audio devices, or stop the Windows Audio service, every time you launch the game. With this fix you don't have to: drop one DLL next to the game and leave your devices alone.

## Install

1. Download `winmm.dll` from the [latest release](../../releases/latest).
2. Put it in the game folder, next to `BlackOps3.exe`. In Steam: right-click the game, **Manage**, **Browse local files**.
3. Launch the game as normal.

To uninstall, delete `winmm.dll` from the game folder. Steam's **Verify integrity of game files** doesn't remove it, since it isn't one of the game's files.

Tested on Windows 11 with the Steam version. It hasn't been tested on Linux or Steam Deck. In theory it works under Proton with the launch option `WINEDLLOVERRIDES="winmm=n,b" %command%`; reports welcome.

## Is it safe?

- It doesn't touch `BlackOps3.exe` or any game file. Windows loads `winmm.dll` from the game folder before the system copy (it isn't on Windows' KnownDLLs list), the same way ReShade and similar mods load.
- Every call is passed straight to the real `C:\Windows\System32\winmm.dll`, except one: when the game asks how many sound mixers exist, the answer is capped at 32.
- Anti-cheat: nobody has checked how Black Ops III's anti-cheat treats an extra DLL. It's only been tested offline (menus, Zombies). Use it online at your own risk.

## What's going on

Black Ops III still uses the old Windows WINMM mixer API to manage your microphone level. Windows creates one mixer for each playback device and each recording device, so every virtual cable adds to the count, and so does each of its inputs and outputs.

About 20 seconds into startup, the game:

1. calls `mixerGetNumDevs`,
2. opens every mixer and looks for recording lines,
3. sets the volume on the recording mixers it found, and repeats that every 20 seconds while running (this is why some people notice the game changing their mic volume).

On the test PC (22 playback and 16 recording devices, 38 mixers), the game hung on the black screen with sound. The hang comes straight after the mixer setup finishes, not during it: the scan itself takes under a tenth of a second. Capping the number of mixers the game is told about found the limit:

| Mixers reported | Result |
|---|---|
| 0 | Starts |
| 16 | Starts |
| 30 | Starts |
| 31 | Starts |
| **32** | **Starts** |
| **33** | **Hangs** |
| 34 | Hangs |
| 38 (all, no fix) | Hangs |

The game most likely keeps mixers in a fixed table of 32 entries and overruns it when there are more. That fits what players report:

- the problem only hits people with many audio devices,
- disabling **recording** devices helps as much as disabling playback ones, because both add mixers,
- stopping the Windows Audio service works, because the game then sees 0 mixers.

This was found on one PC by trimming the end of the mixer list, so the 32 limit is a measurement, not something read from the game's code.

## Settings (optional)

The DLL needs no settings. To change the cap, create `bo3_audio_fix.ini` next to it:

```ini
[fix]
; 32 by default: the game sees at most 32 mixers.
; 0 also works, and stops the game adjusting your microphone volume at all.
; -1 passes the real count through (fix off).
MaxMixers=32
```

With more than 32 mixers, the ones the game no longer sees are the last in Windows' list. The game only uses mixers to set mic volume, so the cost is that it may not adjust one of your microphones. `MaxMixers=0` turns that off completely.

## Debug build

`winmm-debug.dll` in each release (rename it to `winmm.dll`) logs every mixer call the game makes, with timings, to `bo3_audio_fix.log` in the game folder. Useful if the fix doesn't help on your PC and you want to see where startup stops.

## Building

Needs Visual Studio's C++ build tools (x64) and Python 3 with `pefile` (`pip install pefile`).

```bat
build.cmd
```

`tools/gen_exports.py` reads the export table of your system `winmm.dll` and generates a jump stub for each of its 181 exports, so the proxy forwards everything it doesn't change. Output: `build\winmm.dll` and `build\debug\winmm.dll`.

## Credits

The cause was narrowed down from the Steam guide [How to fix Black screen issue (Only sound, no Video) in Black Ops 3](https://steamcommunity.com/sharedfiles/filedetails/?id=3612646328), which worked out that both input and output devices matter, and the "stop Windows Audio" tip in its comments.

## License

MIT
