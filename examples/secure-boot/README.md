# secure-boot

Provision a board so **only your signed, encrypted firmware runs on it** — Flash Encryption (nobody can
read the flash) plus Secure Boot v2 (nobody can run unsigned firmware). See the
[Protect your product](../../docs/recipes/protect-your-product.md) recipe for the background; this
example is the hands-on provisioning flow and the signing-key convention.

> **Both features are irreversible.** Enabling them burns one-time eFuses. Secure Boot can never be
> turned off again, and a lost signing key means the unit can never be updated. Do this only on units
> meant for it, and read the recipe's warnings first.

## The signing-key convention: `deploys/<MAC>.pem`

One Secure Boot signing key **per physical unit**, named by the board's MAC, kept next to the project:

```
examples/secure-boot/
  deploys/
    288485675780.pem          # the signing key for board 28:84:85:67:57:80 — keep it secret
```

The private `.pem` **is** the product: whoever holds it can sign firmware the board will accept.
**Back it up offline** and keep it private. Losing it bricks updates for that unit forever.

## Build it — the key is provisioned automatically

With `secure = true` and `secure_boot = true` in the config, `phpflash build` does everything:

```sh
phpflash build --clean
phpflash flash
```

On build, with the board connected, `phpflash`:

1. reads the board's MAC and ensures `deploys/<MAC>.pem` exists — generating the RSA-3072 signing key
   (kept private at mode 600) the first time (`28:84:85:67:57:80` → `deploys/288485675780.pem`);
2. enables Flash Encryption (dev mode) and Secure Boot v2, signing the bootloader and app with that key;
3. moves the partition table to make room for the signed bootloader (so the flash offsets differ from a
   normal build — always flash with `phpflash flash`, which uses the generated offsets).

An already-present key is reused (one `.pem` in `deploys/` → no need to touch the board); provision a key
ahead of time, or for a board you can't plug in, with `phpflash secure-key --mac 28:84:85:67:57:80`.

On **first boot** the chip burns its encryption key *and* your public-key digest into eFuse, encrypts the
flash, and from then on only your signed images boot.

- **Don't open a serial monitor right after flashing** — on a native-USB board that resets the chip and
  can interrupt the first-boot encryption. Wait ~30–60 s, then connect.
- Prefer Flash Encryption **development mode** (the default) while validating; only move to Release mode
  (`CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y`) for production — Release is one-way.

## Re-flashing afterwards

Just `phpflash build && phpflash flash` again. The build re-signs the images with `deploys/<MAC>.pem`
automatically, and `phpflash flash` notices the chip is already encrypted and switches to
`encrypted-flash` — so the images go in encrypted and signed, with no `esptool --encrypt` or manual
offsets. A plaintext or unsigned flash would not boot, but you never produce one.

## What you end up with

A board that boots only your firmware (Secure Boot) and whose flash — PHP source, `.env`, app — is
unreadable from outside (Flash Encryption). The key to update it lives at `deploys/<MAC>.pem`; guard it.
