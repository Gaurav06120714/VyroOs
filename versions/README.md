# Vyro OS — Version History

One file per major version, in order. Read top-to-bottom to understand
the entire history of Vyro OS from a 512-byte boot sector to a desktop OS
with TLS, NVMe, and three product paths.

| File | Version | Theme | Status |
|------|---------|-------|--------|
| [v1.md](v1.md) | **v1.0** | Bare 64-bit kernel — boots, has a shell, runs user-mode | ✅ Done |
| [v2.md](v2.md) | **v2.0** | Desktop edition — compositor + 12 native apps | ✅ Done |
| [v3.md](v3.md) | **v3.0** | TLS 1.3 + cryptography + real networking (50 sub-versions) | ✅ Done |
| [v4.md](v4.md) | **v4.0** | Polish + glassmorphism + 7 real shipping bug fixes | ✅ Done |
| [v5.md](v5.md) | **v5.0** | Real-hardware detection (AHCI, NVMe, E1000, xHCI, ACPI, USB image) | ✅ Done |
| [v6.md](v6.md) | **v6.0** | Honest audit — 116 PM roadmap, 4 tracks | ✅ Done |
| [v7.md](v7.md) | **v7.0–v7.3** | Tri-path pivot + 69 sub-tags + visible desktop | ✅ Done (current) |

## At a glance

| Major | One sentence |
|-------|-------------|
| v1 | Kernel boots |
| v2 | Has a desktop |
| v3 | Has TLS + networking |
| v4 | Polished + glassmorphism |
| v5 | Real-hardware detection |
| v6 | Honest audit + USB image |
| v7 | Splits into 3 paths + you can run it and see the desktop |

## Total

- **7 major versions**
- **149 git tags**
- **~50,000 lines of code** added cumulatively
- **20+ real shipping bugs** identified and fixed across versions

For the full rule-by-rule compliance audit, see [`docs/RULZ_COMPLIANCE.md`](../docs/RULZ_COMPLIANCE.md).
For the source rules that Vyro is measured against, see [`rulz/`](../rulz/).
