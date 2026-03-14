# Local ncurses bootstrap

This project supports a local ncurses build so Ubuntu/macOS users do not need to install ncurses globally.

Run:

```bash
make deps
```

This downloads and builds ncurses into:

- `third_party/ncurses/local`

Build then uses this local copy automatically.
