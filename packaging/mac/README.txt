MixMate for Mac (AU + VST3, Apple Silicon and Intel)
=====================================================

INSTALL (takes 30 seconds)

1. Open Terminal (press Cmd+Space, type "Terminal", press Enter).
2. Type  bash  followed by a space.
3. Drag the file "Install MixMate.command" from this folder into the
   Terminal window, then press Enter.
4. Restart your DAW. In Ableton, if it doesn't show up:
   Settings > Plug-Ins > Rescan.

(On older macOS you can instead right-click "Install MixMate.command" and
choose Open.)

Why the extra step? MixMate isn't signed with a paid Apple developer
certificate, so macOS blocks it until the installer clears the
"downloaded from the internet" flag. The script only copies MixMate into
your plugin folders and clears that flag - you can open it in TextEdit to
check.


USING MIXMATE

Put MixMate LAST on your master channel.

  N  cycles headphones:  Off (speakers) -> Headphone 1 -> Headphone 2
  M  cycles Stereo -> Mid -> Side
  B  cycles band solo:  Off -> Sub -> Bass -> Mids -> Highs

The keys work while the MixMate window is open and clicked on. To use
them with the window closed, map them in your DAW. In Ableton:
  1. Press Cmd+K (Key Map mode).
  2. Click the "Cycle Headphones" control on the MixMate device (use the
     device's "Configure" button first if it isn't shown).
  3. Press the key you want, then press Cmd+K again to exit.
Do the same for "Cycle Monitor" and "Cycle Solo".

MixMate switches itself off while your DAW exports a bounce (in DAWs that
report offline exports), but it's good practice to set it to Off before
exporting anyway. You can change keys, crossovers and size with the gear
button, and "Save as default" makes every new MixMate start your way.
