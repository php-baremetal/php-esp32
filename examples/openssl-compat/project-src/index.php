<?php
// AES encryption on the microcontroller, with the *compatible* openssl extension: the
// openssl_* functions backed by mbedTLS (the chip's crypto). This is the subset build
// (symmetric ciphers only) -- enough for things like a framework's encrypter.

$key = random_bytes(32);                 // a 256-bit key (real hardware RNG)
$message = "Secret telemetry from an ESP32-P4";

echo "message: $message\n\n";

// --- AES-256-CBC ---------------------------------------------------------------
$iv  = random_bytes(openssl_cipher_iv_length('aes-256-cbc'));   // 16 bytes
$cbc = openssl_encrypt($message, 'aes-256-cbc', $key, 0, $iv);   // base64 out
$dec = openssl_decrypt($cbc, 'aes-256-cbc', $key, 0, $iv);
echo "AES-256-CBC\n";
echo "  ciphertext (base64): $cbc\n";
echo "  decrypted matches:   " . ($dec === $message ? "yes" : "NO") . "\n\n";

// --- AES-256-GCM (authenticated) ----------------------------------------------
$iv12 = random_bytes(12);
$gcm  = openssl_encrypt($message, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, $iv12, $tag, '', 16);
$dec2 = openssl_decrypt($gcm, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, $iv12, $tag);
echo "AES-256-GCM\n";
echo "  auth tag (hex): " . bin2hex($tag) . "\n";
echo "  decrypted matches: " . ($dec2 === $message ? "yes" : "NO") . "\n";

// GCM detects tampering: flip one byte of the tag and decryption must fail.
$badtag = $tag;
$badtag[0] = $badtag[0] ^ "\x01";
$tampered = openssl_decrypt($gcm, 'aes-256-gcm', $key, OPENSSL_RAW_DATA, $iv12, $badtag);
echo "  bad tag rejected:  " . ($tampered === false ? "yes" : "NO") . "\n\n";

// Interop check: these outputs are byte-for-byte what desktop OpenSSL produces.
$flat = openssl_encrypt("test", 'aes-256-cbc', str_repeat("\0", 32), OPENSSL_RAW_DATA, str_repeat("\0", 16));
echo "known-answer (aes-256-cbc, zero key/iv, \"test\"):\n  " . bin2hex($flat) . "\n";
echo "  (desktop OpenSSL: d870798858223f4564d340b103f6527b)\n";
