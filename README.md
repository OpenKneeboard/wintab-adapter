# wintab-adapter

This creates an OTD-IPC v2 server, backed by wintab (vendor tablet drivers).

- It does not work with OpenKneeboard v1.x, as that requires OTD-IPC v1
- It is only known to work well with Wacom v6.4.5
  - newer versions of Wacom's driver intentionally remove support for application-defined expresskeys
  - other manufacturers I've tested do not correctly implement wintab access by non-foreground applications
  - I've specifically tested:
    - the latest version of Huion's driver
    - both the current and osu versions of Gaomon's driver

This is not currently a useful project for anybody; I'm publishing the source just in case that changes in the future.


## What would be needed for this to be useful?

Manufacturers would need to improve their drivers; however, if this is a use-case they care about, their time would be better spent implementing [the OTD-IPC v2 protocol](https://github.com/OpenKneeboard/OTD-IPC/blob/master/docs/protocol.md) instead.

That said, a wintab driver should function with the code as-is if they provide:

- a distinct context handle (`HCTX`) for the current process - some drivers use the same `HCTX` for all applications
- `WM_CTXOVERLAP` messages that:
  - is sent with the correct `wParam` for the the current process
  - is sent when the process becomes inactive, with a *different* `wParam`
  - correct management of `CXS_ONTOP`
- `WTOverlap()` that functions even if the context does not belong to the foreground window
- Tablet/mouse events must always be sent to the active context, even if the mouse cursor is not over the owner window
  - moving the mouse can *change* the active context, but that should result in a `WM_CTXOVERLAP` change that the application can 'undo' with a call to `WTOverlap()`
- More generally, no requirements on the receiving window beyond that it can receive window messages. An inviisble message-only window (parent of `HWND_MESSAGE`) should be sufficient
