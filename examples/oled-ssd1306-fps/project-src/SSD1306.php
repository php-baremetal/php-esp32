<?php
/**
 * SSD1306 128x32 OLED driver, bit-banged over I2C entirely in PHP.
 *
 * The whole link is driven with the `gpio` extension (gpio_mode / gpio_write / gpio_read) --
 * there is no C-side I2C driver. SCL is a permanent push-pull output; the eight data bits drive
 * SDA push-pull, and the ninth (ACK) clock drives SDA low as well (a write-only master that
 * ignores the ACK), which keeps SDA an output for the whole transfer and avoids a slow
 * pin-direction change on every byte. present() is the exception: it releases SDA to read a real
 * ACK, to check the panel is on the bus.
 *
 * The panel is write-only here: a 512-byte framebuffer (128 x 32 / 8) held in PHP and pushed in
 * one I2C transaction per frame. Wire the module's own SDA/SCL pull-ups (the 0.91" boards have
 * them); the bus then idles high.
 */
final class SSD1306
{
    public const WIDTH  = 128;
    public const HEIGHT = 32;

    private const PAGES   = self::HEIGHT >> 3;        // 4 pages of 8 vertical pixels
    private const BUF_LEN = self::WIDTH * self::PAGES; // 512 bytes

    private int $sda;
    private int $scl;
    private int $addr;
    private array $buf;

    public function __construct(int $sda, int $scl, int $addr = 0x3C)
    {
        $this->sda  = $sda;
        $this->scl  = $scl;
        $this->addr = $addr;
        $this->buf  = array_fill(0, self::BUF_LEN, 0);
    }

    /* ---- I2C bit-bang ---------------------------------------------------- */

    private function busInit(): void
    {
        gpio_mode($this->scl, GPIO_OUTPUT); gpio_write($this->scl, 1);
        gpio_mode($this->sda, GPIO_OUTPUT); gpio_write($this->sda, 1);
    }

    private function start(): void
    {
        gpio_write($this->sda, 1); gpio_write($this->scl, 1);
        gpio_write($this->sda, 0); gpio_write($this->scl, 0);   // SDA falls while SCL high
    }

    private function stop(): void
    {
        gpio_write($this->sda, 0); gpio_write($this->scl, 1);
        gpio_write($this->sda, 1);                              // SDA rises while SCL high
    }

    /** Clock out one byte, MSB first; the ninth clock is a driven-low fake ACK. */
    private function writeByte(int $v): void
    {
        $sda = $this->sda;
        $scl = $this->scl;
        for ($i = 0; $i < 8; $i++) {
            gpio_write($sda, ($v >> 7) & 1);
            $v = ($v << 1) & 0xFF;
            gpio_write($scl, 1);
            gpio_write($scl, 0);
        }
        gpio_write($sda, 0);       // ninth clock, SDA held low
        gpio_write($scl, 1);
        gpio_write($scl, 0);
    }

    /** Same, but release SDA on the ninth clock and read the slave's real ACK. */
    private function writeByteAcked(int $v): bool
    {
        $sda = $this->sda;
        $scl = $this->scl;
        for ($i = 0; $i < 8; $i++) {
            gpio_write($sda, ($v >> 7) & 1);
            $v = ($v << 1) & 0xFF;
            gpio_write($scl, 1);
            gpio_write($scl, 0);
        }
        gpio_mode($sda, GPIO_INPUT);            // release; the module pull-up holds it high
        gpio_write($scl, 1);
        $ack = gpio_read($sda) === 0;           // slave pulls low to acknowledge
        gpio_write($scl, 0);
        gpio_mode($sda, GPIO_OUTPUT);
        return $ack;
    }

    /** Does the panel acknowledge its address? Call after the bus is up. */
    public function present(): bool
    {
        $this->busInit();
        $this->start();
        $ack = $this->writeByteAcked($this->addr << 1);
        $this->stop();
        return $ack;
    }

    /** Send one or more command bytes (control byte 0x00). */
    private function command(array $cmds): void
    {
        $this->start();
        $this->writeByte($this->addr << 1);   // address + write
        $this->writeByte(0x00);               // Co=0, D/C#=0 -> the rest is commands
        foreach ($cmds as $b) {
            $this->writeByte($b);
        }
        $this->stop();
    }

    /* ---- panel ----------------------------------------------------------- */

    /** Bring the bus up and run the 128x32 power-on sequence. */
    public function begin(): void
    {
        $this->busInit();
        $this->command([
            0xAE,               // display off
            0xD5, 0x80,         // clock divide / oscillator
            0xA8, 0x1F,         // multiplex ratio = 31 (32 rows)
            0xD3, 0x00,         // display offset 0
            0x40,               // start line 0
            0x8D, 0x14,         // charge pump on
            0x20, 0x00,         // horizontal addressing mode
            0xA1,               // segment remap (flip horizontally)
            0xC8,               // COM scan direction remapped (flip vertically)
            0xDA, 0x02,         // COM pins layout for 128x32
            0x81, 0x8F,         // contrast
            0xD9, 0xF1,         // pre-charge
            0xDB, 0x40,         // VCOMH deselect
            0xA4,               // resume from RAM
            0xA6,               // normal (not inverted)
            0x2E,               // scrolling off
            0xAF,               // display on
        ]);
    }

    /** Push the whole framebuffer: one data transaction of BUF_LEN bytes. */
    public function flush(): void
    {
        $this->command([0x21, 0, self::WIDTH - 1, 0x22, 0, self::PAGES - 1]);   // full window
        $buf = $this->buf;
        $this->start();
        $this->writeByte($this->addr << 1);
        $this->writeByte(0x40);            // Co=0, D/C#=1 -> the rest is data
        for ($i = 0; $i < self::BUF_LEN; $i++) {
            $this->writeByte($buf[$i]);
        }
        $this->stop();
    }

    /* ---- framebuffer drawing --------------------------------------------- */

    public function clear(): void
    {
        $this->buf = array_fill(0, self::BUF_LEN, 0);
    }

    public function pixel(int $x, int $y): void
    {
        if ($x < 0 || $x >= self::WIDTH || $y < 0 || $y >= self::HEIGHT) {
            return;
        }
        $this->buf[(($y >> 3) * self::WIDTH) + $x] |= (1 << ($y & 7));
    }

    public function rect(int $x, int $y, int $w, int $h): void
    {
        for ($j = 0; $j < $h; $j++) {
            for ($i = 0; $i < $w; $i++) {
                $this->pixel($x + $i, $y + $j);
            }
        }
    }

    public function text(int $x, int $y, string $s): void
    {
        $cx = $x;
        $len = strlen($s);
        for ($i = 0; $i < $len; $i++) {
            $glyph = self::FONT[$s[$i]] ?? self::FONT[' '];
            for ($col = 0; $col < 5; $col++) {
                $bits = $glyph[$col];
                for ($row = 0; $row < 7; $row++) {
                    if ($bits & (1 << $row)) {
                        $this->pixel($cx + $col, $y + $row);
                    }
                }
            }
            $cx += 6;   // 5px glyph + 1px spacing
        }
    }

    // A compact 5x7 font (column-major, bit 0 = top row) for the glyphs this demo draws.
    private const FONT = [
        ' ' => [0x00, 0x00, 0x00, 0x00, 0x00],
        '.' => [0x00, 0x00, 0x60, 0x00, 0x00],
        ':' => [0x00, 0x00, 0x14, 0x00, 0x00],
        'x' => [0x44, 0x28, 0x10, 0x28, 0x44],
        '0' => [0x3E, 0x51, 0x49, 0x45, 0x3E],
        '1' => [0x00, 0x42, 0x7F, 0x40, 0x00],
        '2' => [0x42, 0x61, 0x51, 0x49, 0x46],
        '3' => [0x21, 0x41, 0x45, 0x4B, 0x31],
        '4' => [0x18, 0x14, 0x12, 0x7F, 0x10],
        '5' => [0x27, 0x45, 0x45, 0x45, 0x39],
        '6' => [0x3C, 0x4A, 0x49, 0x49, 0x30],
        '7' => [0x01, 0x71, 0x09, 0x05, 0x03],
        '8' => [0x36, 0x49, 0x49, 0x49, 0x36],
        '9' => [0x06, 0x49, 0x49, 0x29, 0x1E],
        'D' => [0x7F, 0x41, 0x41, 0x22, 0x1C],
        'E' => [0x7F, 0x49, 0x49, 0x49, 0x41],
        'F' => [0x7F, 0x09, 0x09, 0x09, 0x01],
        'H' => [0x7F, 0x08, 0x08, 0x08, 0x7F],
        'L' => [0x7F, 0x40, 0x40, 0x40, 0x40],
        'O' => [0x3E, 0x41, 0x41, 0x41, 0x3E],
        'P' => [0x7F, 0x09, 0x09, 0x09, 0x06],
        'S' => [0x26, 0x49, 0x49, 0x49, 0x32],
    ];
}
