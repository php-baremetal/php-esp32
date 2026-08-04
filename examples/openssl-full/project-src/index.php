<?php
// The FULL ext/openssl -- the real OpenSSL 3.0 library, cross-compiled for the chip. This is
// public-key crypto (RSA signatures, encryption), which the mbedTLS `openssl` subset can't do:
// it loads an RSA key and signs, verifies, encrypts and decrypts, all on the microcontroller.

echo "digests (real OpenSSL EVP -- more than the subset offers):\n";
echo "  sha256('abc')    = ", openssl_digest('abc', 'sha256'), "\n";
echo "  sha3-256('abc')  = ", openssl_digest('abc', 'sha3-256'), "\n";
echo "  ripemd160('abc') = ", openssl_digest('abc', 'ripemd160'), "\n\n";

// A 2048-bit RSA key shipped with the example (key.pem next to this file).
$key = openssl_pkey_get_private(file_get_contents(__DIR__ . '/key.pem'));
$d   = openssl_pkey_get_details($key);
echo "loaded an RSA ", $d['bits'], "-bit key\n\n";

// RSA signature: sign with the private key, verify with the public key.
$msg = "signed on an ESP32-P4";
openssl_sign($msg, $sig, $key, OPENSSL_ALGO_SHA256);
$ok = openssl_verify($msg, $sig, $d['key'], OPENSSL_ALGO_SHA256);
echo "RSA-SHA256 sign/verify: ", ($ok === 1 ? "valid" : "INVALID"),
     " (", strlen($sig), "-byte signature)\n";

// RSA encryption: encrypt with the public key, decrypt with the private key.
openssl_public_encrypt("hi chip", $enc, $d['key']);
openssl_private_decrypt($enc, $dec, $key);
echo "RSA public-encrypt / private-decrypt: ", ($dec === "hi chip" ? "OK" : "FAIL"), "\n\n";

// Symmetric ciphers work too (the same functions the subset provides).
$k  = random_bytes(32);
$iv = random_bytes(16);
$ct = openssl_encrypt("secret", 'aes-256-cbc', $k, 0, $iv);
echo "AES-256-CBC roundtrip: ", (openssl_decrypt($ct, 'aes-256-cbc', $k, 0, $iv) === "secret" ? "OK" : "FAIL"), "\n\n";

// Generating a *new* key on the chip works too, because this build ships an openssl.cnf and the
// firmware points OPENSSL_CONF at it (see docs/openssl.md, "Configuration"). RSA-2048 keygen is
// CPU-bound: it takes tens of seconds on the microcontroller (~20-45 s, and it varies).
$t0  = microtime(true);
$new = openssl_pkey_new(['private_key_bits' => 2048, 'private_key_type' => OPENSSL_KEYTYPE_RSA]);
$ms  = (int) round((microtime(true) - $t0) * 1000);
$nd  = openssl_pkey_get_details($new);
echo "generated a fresh RSA ", $nd['bits'], "-bit key on-chip in {$ms} ms\n";
openssl_sign("fresh", $fsig, $new, OPENSSL_ALGO_SHA256);
echo "sign/verify with the new key: ",
     (openssl_verify("fresh", $fsig, $nd['key'], OPENSSL_ALGO_SHA256) === 1 ? "valid" : "INVALID"), "\n";
