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
