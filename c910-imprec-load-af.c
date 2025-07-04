#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <signal.h>
#include <ucontext.h>

void handler(int sig, siginfo_t *info, void *ucontext_voidp) {
	ucontext_t *ucontext = (ucontext_t *) ucontext_voidp;
	unsigned long *regs = ucontext->uc_mcontext.__gregs;

	printf("fault! pc = %#lx\n", regs[0]);
	_Exit(1);
}

int main(int argc, char **argv) {
	if (argc != 2) return 1;
	unsigned long ul = strtoul(argv[1], 0, 0);
	printf("pa = %#lx\n", ul);
	unsigned long page = ul & -4096;
	unsigned long off = ul - page;

	int x = open("/dev/mem", O_RDWR);
	if (x < 0) {
		perror("open");
		return 1;
	}
	void *ptr = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, x, page);
	if (ptr == MAP_FAILED) {
		perror("mmap");
		return 1;
	}
	char *p = (char *)ptr + off;
	printf("va = %p\n", p);
	extern char load_address_is[];

	printf("memory access pc is = %p\n", load_address_is);

	struct sigaction sa = { 0 };
	sa.sa_flags = SA_SIGINFO;
	sa.sa_sigaction = handler;

	// Uncomment to see epc in userspace
	// sigaction(SIGSEGV, &sa, NULL);

	asm volatile (
	"load_address_is:\n\t"
		"lb x0, (%0)\n\t"
		"fence.i"
		:
		: "r"(p)
		: "memory"
	);
	return 0;
}
