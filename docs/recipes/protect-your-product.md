---
eyebrow: 'Docs · Recipes'
lede:    'Stop a field device being cloned or read: Flash Encryption hides the baked PHP source and .env, Secure Boot v2 makes only your signed firmware run. One opt-in flag plus a one-time, irreversible provisioning on each unit.'
see_also:
  - href: ./power-save.md
    meta: 'Recipes'
    label: 'Power save'
  - href: ../extensions/builtin-api.md
    meta: 'Extensions'
    label: 'Built-in extension API'
  - href: ../reference/partitions.md
    meta: 'Reference'
    label: 'Partitions and the flash layout'
prev:
  label: 'Power save'
  href: ./power-save.md
next:
  label: 'No next page'
  href: '#'
---

# Protect your product

## The problem

By default an ESP32 board is fully readable. Anyone who connects it can dump the flash and read your
baked PHP source and your `.env` values in plaintext:

<!-- @code-block language="bash" label="what an attacker can do today" -->
```bash
esptool read_flash 0 0x400000 dump.bin
strings dump.bin | grep -i secret     # finds your .env values, your PHP, ...
```
<!-- @endcode-block -->

Two hardware features close this:

- **Flash Encryption** — a key in eFuse (read-protected, never leaves the chip) encrypts the flash. A
  dump is noise, and reflashing it onto another chip is useless.
- **Secure Boot v2** — only firmware signed with your offline private key boots.

<!-- @callout variant="danger" title="These burn eFuses — they are permanent" -->
Enabling either feature burns one-time fuses on the chip. Release mode is one-way and disables the plain
serial download path. A wrong setting, or a lost signing key, can brick a unit or make it impossible to
update ever again. Do this only on units meant for it; keep developing on ordinary, unprotected boards.
<!-- @endcallout -->

<!-- @callout variant="note" title="Permanent, but not bricked" -->
Enabling Flash Encryption is not reversible: once the key is burned on first boot you cannot un-encrypt
the flash, read the plaintext back, or return the chip to a virgin state. But the board is **not**
bricked — it stays fully usable as a permanently-encrypted board, and in development mode you can keep
re-flashing it with encrypted images. You simply can't ever go back to a plaintext chip. (Only a wrong
config, a lost signing key, or Release mode's lockdown is what can leave a unit unusable.)
<!-- @endcallout -->

## What gets protected

Flash Encryption covers the firmware image automatically; a *data* partition is only covered when it is
flagged `encrypted`, which the `secure` flag does for the baked PHP source.

| What | Where it lives | Under Flash Encryption |
|---|---|---|
| `.env` (baked) | app image (`.rodata`) | yes — automatic, the app is always encrypted |
| App / engine / extensions | app partition | yes — automatic |
| PHP source (embedded) | `storage` partition (FAT) | yes — with the `encrypted` flag, added by `secure = true` |
| `store_*` (runtime) | `phpstore` (NVS) | no — not covered, needs NVS Encryption |
| PHP source on microSD | external SD card | no — never, the card is plaintext |

So `secure = true` protects the baked PHP source and the `.env`: the `.env` is already inside the
auto-encrypted app, and the flag adds the `encrypted` flag the PHP-source partition needs. For a protected
product keep the source **embedded**, not on microSD.

## The two features side by side

They solve different problems and you usually want both. Neither can be undone.

| | Flash Encryption (`secure`) | Secure Boot v2 (`secure_boot`) |
|---|---|---|
| **Protects against** | reading the flash — source, `.env`, app come out as ciphertext | running *unsigned* firmware — clones and tampered images won't boot |
| **Reversible?** | No. You can't un-encrypt or return the chip to a virgin state — but it stays usable and re-flashable (encrypted) | No, and stricter: `SECURE_BOOT_EN` is one-way, the chip **forever** boots only your signed firmware |
| **What you must keep** | nothing in the default per-device dev mode (the key is generated on-chip, read-protected) — only a *shared* FE key, if you chose that model | **the signing key `deploys/<MAC>.pem`** — always, offline |
| **If you lose that** | nothing to lose in dev mode; re-flash with `--encrypt` and carry on | that unit accepts **no update, ever** — it's stuck on its last signed firmware |

**Both together** = a product whose flash is unreadable *and* which runs only your firmware. Both burns
are permanent, so provision on units meant for it. The one irreplaceable secret is the **Secure Boot
signing key**: back up every `deploys/<MAC>.pem` offline — losing it ends updates for that board.

## Step 1 — Flash Encryption (the `secure` flag)

Turn it on in `php-esp32.config.toml`:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
secure = true
```
<!-- @endcode-block -->

A `secure` build does two things:

- marks the embedded `storage` partition **`encrypted`** — data partitions aren't encrypted by default,
  so without this the baked PHP source would stay in plaintext even with Flash Encryption on;
- enables Flash Encryption in **development mode** in the firmware's sdkconfig.

The `.env` needs nothing extra: it is compiled into the app image, which is always encrypted.

Build and flash as usual:

<!-- @code-block language="bash" label="first flash" -->
```bash
phpflash build --clean
phpflash flash
```
<!-- @endcode-block -->

On the **first boot** the bootloader generates the encryption key, burns it into eFuse, and encrypts the
flash in place. From then on the contents are unreadable from outside.

<!-- @callout variant="warning" title="Don't interrupt the first boot" -->
The first-boot encryption must run **undisturbed** — it takes a few seconds and resetting the chip
part-way can leave it in a boot loop (`invalid header`). On boards with a native USB-Serial/JTAG (e.g. an
S3-Zero), opening a serial monitor **resets the chip**, so don't open one right after flashing: flash,
wait ~30–60 s, then connect. (If it does loop and the eFuse key wasn't burned yet, you can recover by
re-flashing plaintext.)
<!-- @endcallout -->

<!-- @callout variant="note" title="Re-flashing is still just phpflash flash" -->
The first flash is written plaintext and encrypted on first boot; later flashes must be written
**encrypted**. `phpflash flash` handles this for you — it detects that the chip is already encrypted and
switches to `encrypted-flash` (the chip encrypts in hardware with its own key). Development mode exists
precisely so you can keep re-flashing while testing; Release mode (below) locks that down.
<!-- @endcallout -->

## Step 2 — Verify it worked

Dump the flash again and confirm your secrets are now noise, not text:

<!-- @code-block language="bash" label="verify" -->
```bash
esptool read_flash 0 0x400000 dump.bin
strings dump.bin | grep -i secret     # now finds nothing — the data is encrypted
```
<!-- @endcode-block -->

Compare against a dump of the same firmware built **without** `secure = true`: there the string is
plainly visible.

`phpflash discover` also reads the chip's eFuses and reports the protection state directly:

<!-- @code-block language="text" label="phpflash discover" -->
```
Flash encryption: enabled
Secure boot:      enabled
```
<!-- @endcode-block -->

## Step 3 — Secure Boot v2

Flash Encryption stops reading; Secure Boot stops running *unsigned* firmware. Turn it on with a second
flag:

<!-- @code-block language="toml" label="php-esp32.config.toml" -->
```toml
secure = true
secure_boot = true
# secure_boot_keys_dir = "./deploys"   # where <MAC>.pem lives (default: ./deploys)
```
<!-- @endcode-block -->

The convention is **one signing key per unit, stored as `deploys/<MAC>.pem`** — a private secret you keep
offline. `phpflash build` provisions it for you: with the board connected it reads the MAC, generates
`deploys/<MAC>.pem` (RSA-3072, kept private at mode 600) the first time, then signs the bootloader and app
with it. An existing key is reused (so later builds don't touch the board). To pre-provision a key without
the board, run `phpflash secure-key --mac 28:84:85:67:57:80`.

On first boot the chip burns the **public-key digest** into eFuse, and from then on only images signed
with your key boot. (Secure Boot moves the partition table to `0xc000` to fit the signed bootloader, so
the flash offsets change — flash with `phpflash flash`.)

<!-- @callout variant="danger" title="The signing key is the product" -->
Lose `deploys/<MAC>.pem` and you can never sign another firmware for that unit — it will accept no
update, ever. Secure Boot can never be turned off once burned. Back the key up offline and keep it
private; anyone who has it can sign firmware your board will run.
<!-- @endcallout -->

## Development vs Release mode

- **Development mode** (what `secure = true` sets): the chip is encrypted, but the serial download path
  still works so you can keep flashing. Use it to validate the whole flow.
- **Release mode**: one-way. The plaintext serial download path is disabled and the encryption settings
  are locked. This is the production setting — set
  `CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y` and burn the units you ship, once you're confident.

## Key model

- **Per-device key, generated on-chip** (the development-mode default): strongest, every unit is unique,
  but you can't pre-build one encrypted image for all units.
- **Shared, pre-generated key**: lets you flash one encrypted image across a production run (generate with
  `espsecure.py generate_flash_encryption_key`), at the cost of a single key for the whole fleet.

For anti-cloning, prefer per-device.

## What is not covered

- **`store_*` (the persistent store)** lives in an NVS partition, which Flash Encryption does **not**
  cover. Secrets written there at runtime need separate NVS Encryption (a dedicated `nvs_keys`
  partition) — not set up by the `secure` flag.
- **PHP source on a microSD card is never protected** — the card is external and plaintext. For a
  protected product, bake the source into the (encrypted) firmware (`storage_type = "embedded"`), not on
  SD.

## Hardware note

For the strongest Secure Boot, use an ESP32 **ECO3** or a modern ESP32-S3 / C-series chip. A brand-new
chip is almost always fine; only very old stock may predate the needed revision.

## Commands: before vs after

The commands don't change — it's the **same `phpflash build` and `phpflash flash`** throughout. The
flags steer what each one does; `phpflash` adapts (provisioning the key, signing, and switching to
encrypted flashing) on its own.

| | Plain build | `secure = true` (Flash Encryption) | `+ secure_boot = true` (Secure Boot) |
|---|---|---|---|
| Build | `phpflash build` | `phpflash build` | `phpflash build` — provisions `deploys/<MAC>.pem`, signs |
| First flash | `phpflash flash` | `phpflash flash` (plaintext, encrypted on first boot) | `phpflash flash` (encrypted + signed) |
| Re-flash | `phpflash flash` | `phpflash flash` (auto `encrypted-flash`) | `phpflash flash` (auto `encrypted-flash`, signed) |
| Read the flash? | yes (`strings`) | no — ciphertext | no — ciphertext |
| Run unsigned firmware? | yes | yes | no |

What `phpflash` does under the hood so you don't have to:

- **Build**: with `secure` it marks the `storage` partition encrypted; with `secure_boot` it provisions
  `deploys/<MAC>.pem`, signs the bootloader and app, and shifts the partition table (so the flash offsets
  move — which is exactly why you let the tool handle flashing).
- **Flash**: on a virgin chip it writes plaintext (the chip encrypts itself on first boot); once the chip
  is encrypted it detects that and runs `encrypted-flash`, so the images go in through the chip's own
  hardware encryption. You never type an `esptool --encrypt` line or chase offsets by hand.
