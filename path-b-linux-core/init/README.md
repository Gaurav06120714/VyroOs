# vyro-init

Vyro PID 1 for Path B. Brings up the Linux side just enough to get the
Vyro display server running and then launches a configurable session.

## Boot sequence

1. Mount `/proc`, `/sys`, `/dev`, `/run`, `/tmp` with the right flags
2. `mkdir -p /run/vyro` so the compositor's socket has a home
3. `sethostname("vyro")`
4. Spawn `/usr/bin/vyro-compositor` (the display server + IPC + chrome + input)
5. Wait up to 10s for `/run/vyro/compositor.sock` to appear
6. Walk `/etc/vyro/session.d/*.cmd` in sorted order, fork+exec each
7. Loop on `wait()` — respawn the compositor if it dies, log session children

## Session drop-ins

Each `/etc/vyro/session.d/*.cmd` file contains **one** command line:

```
/usr/bin/vyro-files /root
```

Numeric prefixes order the autostarts: `10-foo.cmd` runs before `20-bar.cmd`.

## Defaults shipped

- `10-files.cmd` — `/usr/bin/vyro-files /root`
- `20-calculator.cmd` — `/usr/bin/vyro-calculator`

So a clean Path B image boots straight into the desktop with Files at
`/root` and Calculator already open.

## What this is NOT

- Not a systemd replacement for general Linux use
- No socket activation
- No service dependencies beyond "compositor first, autostart second"
- No logging beyond stderr
- No journal

If you need any of those, run something else as PID 1.
