	.global main
main:
	mv		t0,	sp

	# th.lbib	t0, 	(t0),	0,	0
	.word 0x0802c28b
	frcsr		t0
	addi		t0,	zero,	0

	ret
