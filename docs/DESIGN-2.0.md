# Vyro OS 2.0 — Desktop Architecture

This document describes the desktop layer added in Phases 31-50, sitting on
top of the v1.0 kernel.

## High-level layering

```
┌────────────────────────────────────────────────────────────┐
│                  DESKTOP APPLICATIONS                      │
│  Files  Settings  Terminal  TextEdit  Calc  Clock          │
│  Launchpad  TaskMgr  Notifications  Control  Browser  Store│
├────────────────────────────────────────────────────────────┤
│                  APPLICATION FRAMEWORK                     │
│  app_def_t (name, icon, render fn, default size)           │
│  app_ctx_t (mouse, keyboard, dimensions, origin)           │
├────────────────────────────────────────────────────────────┤
│                    WIDGET TOOLKIT                          │
│  button  label  panel  toggle  progress  input  list  tile │
├────────────────────────────────────────────────────────────┤
│              WINDOW MANAGER  +  SHELL                      │
│  Top bar  Dock  Launchpad  Notifications                   │
│  Window: drag/min/max/resize/snap/z-order/focus            │
├────────────────────────────────────────────────────────────┤
│                COMPOSITOR (back-buffered)                  │
│  rect  border  shadow  glyph  text  gradient  blit         │
│  Theme: dark/light palette                                 │
├────────────────────────────────────────────────────────────┤
│              FRAMEBUFFER + MOUSE + KEYBOARD                │
│  VESA 1024x768x24bpp, PS/2 IRQ12, PS/2 IRQ1                │
├────────────────────────────────────────────────────────────┤
│                  V1.0 KERNEL (unchanged)                   │
└────────────────────────────────────────────────────────────┘
```

## Rendering pipeline (per frame, ~60 fps)

```
gui_run() loop iteration:
   poll keyboard → key event
   poll mouse → cursor position + buttons
   dispatch click → window manager
   for each window (back to front):
       call window's app render(ctx)
       app draws into compositor back buffer
   draw top bar with live clock
   draw dock + hover tooltips
   draw notification toasts
   draw cursor (always on top)
   comp_present()  → blit entire back buffer to framebuffer
   sleep_ms(16)    → ~60 Hz
```

The back buffer (2.25 MB allocated from `kmalloc`) is the key to flicker-free
rendering, animations, and proper window compositing. Every frame is drawn
fully off-screen, then copied to the visible framebuffer in one shot.

## Adding a new app

1. Create `kernel/apps/myapp.c`
2. Implement `static void render(app_ctx_t* ctx)` using widgets
3. Export `const app_def_t APP_MYAPP = { "MyApp", 'M', 0xFFFFFF, render, 400, 300 };`
4. Register in `apps.c`: `app_register(&APP_MYAPP);`
5. Done — appears in Launchpad, can be opened from any code via `open_app("MyApp")`.

## IPC primitives (Phase 49)

- **Pipes** — circular byte buffer per pipe, `pipe_write/read` move bytes
- **Message queues** — fixed-size payload slots, `mq_send/recv` move messages
- **Signals** — POSIX-style numeric signals, accounting only (delivery is a v3 item)

## Sockets API (Phase 46)

Berkeley-style `socket/bind/connect/listen/send/recv/close` with a real
**TCP state machine** following RFC 793. Transport (NIC TX/RX) is a stub in
2.0 — real wire transmission requires the RTL8139 driver, scoped for v3.0.

## Theme system

Every UI color is named in `theme_t`. Two palettes (dark, light) shipped.
Apps and the compositor only reference `theme()->color_name`, never raw RGB.
Press `T` in the GUI to flip — every widget, every app, every notification
re-themes instantly.
