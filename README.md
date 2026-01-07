# wintab-adapter

This creates an OTD-IPC v2 server, backed by wintab (vendor tablet drivers).

It does not work with OpenKneeboard v1.x, as that requires OTD-IPC v1

This is not currently a useful project for anybody.

## What does it do?

It makes WinTab drivers usable from OTD-IPC v2 applications, such as future versions of OpenKneeboard.

It also works around several common driver bugs:

- Broken or missing support for non-foreground windows via `WTOverlap()` and `WT_OVERLAP`
- Incomplete or buggy support for axis scaling and unit mapping via the `lcOut` fields
- Missing or buggy support for inverting the Y axis

## What doesn't it do?

It does not support OpenKneeboard v1.x.

It is able to work around some bugs in the drivers, but it's not able to add completely missing features.

| Driver           | Correct tablet area | Pen buttons | Tablet buttons |
|------------------|---------------------|-------------|----------------|
| Wacom v6.4.12    | ✅                   | ❌           | ❌              |
| Huion v15.7.6    | ❌                   | ❌           | ❌              |
| Gaomon v14.8.133 | ❌                   | ❌           | ❌              |

## I need those features!

These features are usable with this adapter when they are made available by the hardware driver; only the driver manufacturer can add support for them on your tablet.

Your options are:

- Use OpenTabletDriver instead
- If you have a Wacom tablet which is supported by driver v6.4.5, use that instead; these features are available there.
  Wacom informed me these features were intentionally removed in later versions
- You can ask your tablet manufacturer to implement [OTD-IPC v2](https://github.com/OpenKneeboard/OTD-IPC/blob/master/docs/protocol.md) so you can use their driver without this adapter
- You can ask your tablet manufacturer to add the missing features to their WinTab driver

## How do I use this?

**DON'T.**

- still in development, and not suitable for use by *ANYONE*
- OpenTabletDriver is *much* better than any vendor driver I'm aware of

That said, it's a console application. Once you've built it and a compatible client:

- **Wacom:**
  - Run `wintab-adapter-64.exe`
  - Administrator *not* required
  - If your driver has these options, set all pen and expresskey buttons to 'Application Defined'
  - If not, you may want to set them to 'disabled' so you don't accidentally trigger other actions
- **Huion:**
  - Run `wintab-adapter-64.exe --hijack-buggy-driver=Huion`
  - Administrator *sometimes* required:
    - The Huion driver is inconsistent about whether or not it runs elevated
    - If the Huion driver is running elevated, you must run `wintab-adapter-64.exe` elevated
    - Otherwise, either way works
  - Set the screen mapping to 'All Display'
  - You may want to set pen and button bindings to 'disabled' so you don't accidentally trigger other actions
- **Gaomon:**
  - Run `wintab-adapter-32.exe --hijack-buggy-driver=Gaomon`
  - Administrator *always* required
  - Set the screen mapping to 'All Display'
  - You may want to set pen and button bindings to 'disabled' so you don't accidentally trigger other actions
- **All others:**: `wintab-adapter-64.exe` *may* work for you. If not, you need to use OpenTabletDriver instead

## What does `--hijack-buggy-driver` do?

It works around buggy or incomplete implementations of `WTOverlap()` and `WT_OVERLAP`, allowing the adapter to work when it is not the foreground window.

If you're familiar with OpenKneeboard's "invasive" mode, this solves the same problem by doing roughly the opposite:

- 'invasive' mode modifies other programs (e.g. games) so that when they are in the foreground, they load the tablet driver and forward the data to OpenKneeboard
- `wintab-adapter` instead modifies the driver so that it never realizes that other programs (like the games) are in the foreground

These drivers repeatedly ask Windows what the foreground Window is, and/or the Window under the mouse cursor is. On my system, this happens between 700 and 1500 times per second.

The `--hijack-buggy-driver` option intercepts the following calls so that the driver always thinks that the `wintab-adapter` window is the foreground window:

- `GetForegroundWindow()`
- `WindowFromPoint()`

This has the advantages that:

- it's simpler
- anti-cheat is less likely to have issues with it
- buggy drivers are much less likely to crash other apps (games)


