# Keke OS Design Philosophy

> **Keke OS is an Owner OS, not a consumer OS.**

A consumer OS is a finished puzzle. The owner opens the box and starts
using it immediately — every piece is already placed, every default is
already decided, and the system quietly makes choices on your behalf.

Keke OS is the opposite. It is a puzzle **you design the pieces for**,
and **you decide where every piece goes**. Nothing exists on the system
because an installer checkbox decided it should. It exists because you
explicitly put it there.

This is not a statement that consumer OSes are bad. They serve a lot of
people well. They just are not for us — not for anyone who chose Arch,
and not for anyone who chooses Keke OS.

---

## The Test

> **"Does this feature respect the user's explicit choice,
> or does it quietly decide something for them?"**

Every design decision in Keke OS is measured against this question:

- **Nothing runs or installs unless you asked for it.** No forced
  services, no bundled apps, no telemetry-style background processes.
  The default state is *empty* — everything else is a deliberate choice.
- **The command line is not the point.** The point is that every single
  thing on the system exists because you specifically decided to put it
  there. The shell-first design is simply the honest way to surface
  those decisions.
- **The GUI is something you launch, not something the system needs.**
  The XP/Mac-style desktop stays architecturally optional — the same
  relationship Xorg/Wayland and GNOME have to Arch. The OS must work
  fine without it, because the OS is the shell, the init, and the
  pieces the owner chose.
- **KPM's job is pacman's job:** let the user assemble precisely what
  they want, nothing more, nothing hidden. The package manager is a
  tool for expressing intent, not a vehicle for defaults.

---

## What This Means In Practice

- Shell-first, GUI-optional. The GUI is a launched program, never a
  hard dependency of the OS.
- No background daemons, no "helpful" auto-services, no update agents
  the owner never asked to run.
- Every shipped piece is understandable end-to-end. If a user cannot
  explain why a component exists, that component fails the philosophy.
- Honesty over convenience. Keke OS may ask more of the user, but it
  never lies about what the system is doing.

---

## In One Line

> We are not building a puzzle someone else solved for you.
> We are building the puzzle — and you decide what fits where.
