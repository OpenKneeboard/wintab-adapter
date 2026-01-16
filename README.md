# wintab-adapter

This makes WinTab drivers usable from OTD-IPC v2 applications, such as future versions of OpenKneeboard. There is opt-in support for OTD-IPC v1, [including OpenKneeboard v1](#using-with-openkneeboard-v1).

It also works around several common driver bugs:

- Broken or missing support for non-foreground windows (`WTOverlap()` and `WT_OVERLAP`)
- Incomplete or buggy support for axis scaling and unit mapping via the `lcOut` fields
- Missing or buggy support for setting the Y axis orientation

## What doesn't it do?

It is able to work around some bugs in the drivers, but it's not able to add completely missing features.

| Driver                       | Correct tablet area | Pen buttons | Tablet buttons |
|------------------------------|---------------------|-------------|----------------|
| Wacom v6.4.12                | ✅                   | ❌           | ❌              |
| Huion v15.7.6                | ❌                   | ❌           | ❌              |
| Huion "3ExpressKey_0Softkey" | ❌                   | 🐛          | ❌              |
| Gaomon v14.8.133             | ❌                   | ❌           | ❌              |
| XP-Pen v4.0.12               | ❌                   | ❌           | ❌              |
| Wacom v6.4.5                 | ✅                   | ✅           | ✅              |
| OpenTabletDriver             | ✅                   | ✅           | ✅              |

❌: indicates a limitation imposed by the manufacturer's driver, not this adapter.

🐛: indicates the driver provides this feature, but it is unreliable

These versions are the latest as of 2026-01-07

Huion "3ExpressKey_0SoftKey" is a driver published in 2018, and is still the latest available for some tablets such as
the Huion H420, a.k.a. '420 OSU'.

## I need those features!

These features are usable with this adapter when they are made available by the hardware driver; only the driver
manufacturer can add support for them on your tablet.

Your options are:

- Use OpenTabletDriver instead; *strongly* recommended
- Try older versions of your manufacturer's driver
  - For Wacom specifically, if your tablet is supported by v6.4.5, use that instead; Wacom informed me that WinTab control of buttons was intentionally removed in later versions
- You can ask your tablet manufacturer to implement [OTD-IPC v2](https://github.com/OpenKneeboard/OTD-IPC/blob/master/docs/protocol.md) so you can use their driver without this adapter
- You can ask your tablet manufacturer to add the missing features to their WinTab driver

## How do I use this?

**DON'T.**; use OpenTabletDriver instead if at all possible.

That said, you can download the latest version from [releases](https://github.com/OpenKneeboard/wintab-adapter/releases/latest) (you don't need the debug symbols), then:

- **Wacom:**
    - Run `wintab-adapter-64.exe`
    - Administrator is *not* required
    - If your driver has these options, set all pen and expresskey buttons to 'Application Defined'
    - If not, you may want to set them to 'disabled' so you don't accidentally trigger other actions
- **Huion:**
    - Try both:
        - `wintab-adapter-64.exe --hijack-buggy-driver=Huion` (for most tablets)
        - `wintab-adapter-64.exe --hijack-buggy-driver=HuionAlternate` (for some older models)
    - Administrator *sometimes* required:
        - The Huion driver is inconsistent about whether it runs elevated
        - If the Huion driver is running elevated, you must run `wintab-adapter-64.exe` elevated
        - Otherwise, either way works
    - Set the screen mapping to 'All Display'
        - in some versions of the driver, this is labeled as 'All desktop area'
    - You may want to set pen and button bindings to 'disabled' so you don't accidentally trigger other actions
        - in some versions of the driver, this is labeled as 'none'
    - Turn off rotation
- **Gaomon:**
    - Run `wintab-adapter-32.exe --hijack-buggy-driver=Gaomon`
    - Administrator *always* required
    - Set the screen mapping to 'All Display'
    - You may want to set pen and button bindings to 'disabled' so you don't accidentally trigger other actions
    - Turn off rotation
- **XP-Pen:**
    - Run `wintab-adapter-32.exe --hijack-buggy-driver=XPPen`
    - Administrator is *not* required
    - Set the screen mapping to 'All Monitor' and "Set full screen"
    - Turn off rotation
- **All others:** Try `wintab-adapter-64.exe` without any options; this will work with any correctly implemented WinTab driver. If this doesn't work, your options are:
    - Try all the options above for other manufacturers; many tablet brands are just different names for the same manufacturer
    - Use OpenTabletDriver
    - Contact your manufacturer and ask them to fix their WinTab driver

### Using with OpenKneeboard v1

- run the adapter with `--otd-ipc-v1` in addition to any other options you need (see above)
- in OpenKneeboard Settings → Input, configure OpenKneeboard to use OpenTabletDriver instead of WinTab, as this adapter pretends to be OpenTabletDriver

It is likely that no buttons will work [due to missing features in vendor drivers](#what-doesnt-it-do), so you will be
unable to erase or bind anything; however, the pen tip should work.

## What does `--hijack-buggy-driver` do?

It works around buggy or incomplete implementations of `WTOverlap()` and `WT_OVERLAP`, allowing the adapter to work when
it is not the foreground window.

If you're familiar with OpenKneeboard's "invasive" mode, this solves the same problem by doing roughly the opposite:

- 'invasive' mode modifies other programs (e.g. games) so that when they are in the foreground, they load the tablet
  driver and forward the data to OpenKneeboard
- `wintab-adapter` instead modifies the driver so that it never realizes that other programs (like the games) are in the
  foreground

These drivers repeatedly ask Windows what the foreground Window is, and/or the Window under the mouse cursor is. On my
system, this happens between 700 and 1500 times per second.

The `--hijack-buggy-driver` option intercepts the following calls so that the driver always thinks that the
`wintab-adapter` window is the foreground window:

- `GetForegroundWindow()`
- `WindowFromPoint()`

This has the advantages that:

- it's simpler
- anti-cheat is less likely to have issues with it
- buggy drivers are much less likely to crash other apps (games)
- If the game is running as administrator, this is *more likely* to work. OpenKneeboard remains untested with this configuration, and no help is available
