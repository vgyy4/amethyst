# checkra1n → Amethyst migration button

Adds a **"Migrate from checkra1n"** button so Amethyst can take over a device that
is *currently* jailbroken with checkra1n, instead of refusing to run because it
detects an existing jailbreak.

## Why this is needed

On a device you can't sign apps for (no trusted certificate), the only way to get
the Amethyst app to launch is under checkra1n + AppSyncUnified. But stock Amethyst
sees checkra1n's jailbreak, prints "Jailbroken", and disables its button — and it
won't run its own kernel exploit on a kernel checkra1n has already exploited.

## What it does

1. Detects that the active jailbreak is **not** Amethyst (no `/.installed_amethyst`)
   and relabels the main button to **Migrate from checkra1n** (instead of locking it).
2. On press, **validates first (read-only)**: it reuses checkra1n's kernel task port
   (`host` special port 4) to fill in `kinfo` — kernel base/slide, kernel/self task
   and proc, realhost, ipc_space_kernel — and runs Amethyst's patchfinder to prove
   every offset resolves. Nothing is written to the kernel or disk in this step.
3. Shows you the derived kernel values and asks to confirm.
4. On confirm, runs Amethyst's **normal, existing** jailbreak pipeline with the new
   `JB_FLAG_MIGRATE_CHECKRA1N` flag (which skips the exploit and uses the handoff).
   **checkra1n 0.12.4 uses the bingner/Substrate bootstrap (Cydia), which is NOT
   Procursus**, and Amethyst can only *adopt* a Procursus bootstrap. So when the
   active bootstrap isn't Procursus (the checkra1n case), the migrate flow **forces
   `JB_FLAG_RESTORE_ROOTFS`**: it wipes checkra1n/Cydia and installs a fresh
   Amethyst/Procursus bootstrap using the borrowed kernel access. If a Procursus
   bootstrap is detected instead, it is adopted in place with no wipe.
5. Userspace-reboots into Amethyst.

## Files changed

| File | Change |
|------|--------|
| `jailbreak/jailbreak.h` | `JB_FLAG_MIGRATE_CHECKRA1N`; declares `checkra1n_handoff()` / `validate_checkra1n_migration()` |
| `jailbreak/jailbreak.c` | `checkra1n_handoff()`, `migrate_find_kernel_base()`, `validate_checkra1n_migration()`; `run_exploit()` branch on the flag |
| `ui/MainController.h` | `isForeignJailbreak`, `extraJBFlags` properties |
| `ui/MainController.m` | foreign-JB detection, button relabel, `migrateButtonAction`, split of `jailbreakButtonAction` into `runJailbreakFlow` |
| `.github/workflows/build.yml` | CI that builds the unsigned IPA on a macOS runner |

## ⚠️ Risks — read before running on a device you care about

- This is **untested kernel code**. It was written against the source but never run.
  Test on **one** of your spare iPad mini 2s first, not all five.
- **The linchpin is unverified:** `checkra1n_handoff()` assumes a sandboxed,
  AppSync-installed app can reuse checkra1n's kernel task port via
  `host_get_special_port(…, 4, …)` (or `task_for_pid(0)`). checkra1n does place the
  kernel task port in `realhost.special[4]`, but the documented mechanism targets
  root/entitled processes, and AppSync grants no entitlements. **The read-only
  validation step exists specifically to test this** — if it reports "could not
  borrow checkra1n's kernel task port," the whole approach is a non-starter on this
  setup and nothing destructive runs.
- **Destructive by necessity on checkra1n:** because checkra1n is bingner/Substrate
  (not Procursus), migration **restores RootFS** — checkra1n, Cydia, and every
  tweak/package go away, replaced by a clean Amethyst bootstrap. Back up first.
- The one genuinely new/risky piece is `checkra1n_handoff()`. The validation step
  exists specifically so you can confirm the kernel values look sane (plausible
  `0xfffffff0…` kernel base, correct pid) **before** the destructive part. If
  validation fails, **stop** — do not proceed.
- Wrong kernel offsets on a live device can **panic / require a DFU restore**. You
  know this; just back up first.
- **Cold-boot limitation (unchanged by this):** Amethyst is semi-*un*tethered and
  the app is unsigned, so after a full power-off it still won't relaunch without
  checkra1n/AppSync. Migration gives you an Amethyst session now; it does not make
  the unsigned app survive a cold boot. (Installing AppSyncUnified from Amethyst's
  Sileo afterward is the way to keep unsigned apps — including Amethyst — launchable
  under the Amethyst jailbreak.)
- `mach_port_kobject` is a private routine. If CI fails to compile on its prototype,
  that's the one line to adjust to match the SDK header.

## Building

Push to your fork → the **Build Amethyst IPA** GitHub Action runs → download the
`Amethyst-ipa` artifact. Or locally on the Hackintosh: `cd Amethyst && bash buildipa.sh`.
