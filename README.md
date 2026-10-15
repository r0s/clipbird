ClipBird clipboard utility for windows.

The app runs without showing a window, starts in the Windows notification area,
and listens for clipboard updates. When copied content includes an image, it
keeps the image available for pasting into apps and also adds a PNG file format
so it can be pasted into File Explorer as a new file. The PNG is stored in the
ClipBird folder under the Windows temporary directory and is removed when
the clipboard is next successfully replaced while the app is running. If the
app exits while that image is still on the clipboard, the PNG is retained so
the clipboard remains usable.

When copied content does not include an image but includes text, the app
replaces the clipboard formats with plain Unicode text, preserving the text
without rich formatting. Clipboard contents are not logged or displayed.

Tray controls:
- The tray icon image is embedded in the executable from `Parrot.png`.
- The tray icon tooltip indicates how the latest clipboard item was handled.
- The tray menu follows the Windows app light or dark theme.
- Use "Convert clipboard text to plaintext." to toggle text conversion.
- Use "Let screenshots be pastable to explorer." to toggle adding PNG file data
  for clipboard images. Both options are enabled by default.
- Choose Exit from the tray menu to stop monitoring and close the app.

The app uses an invisible form handle to receive Windows clipboard update
messages. The clipboard listener is registered with that handle and removed
when the app closes.

Build and debug:
- In VS Code, run the default build task, "MSVC: Build ClipBird".
- Use the "ClipBird (MSVC)" launch configuration to build and debug.
- These configurations require Visual Studio Build Tools 2022, the MSVC C++
  toolchain, the .NET Framework 4.8 targeting libraries, and the Windows SDK.
- `Parrot.png` is needed beside the source files when building; it is not
  needed beside `ClipBird.exe` when running the app.
