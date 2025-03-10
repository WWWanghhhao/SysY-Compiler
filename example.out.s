	.text
	.globl main
	.attribute arch, "rv64i2p1_m2p0_a2p1_f2p2_d2p2_c2p0_zicsr2p0_zifencei2p0_zba1p0_zbb1p0"
main:
.main_0:
	sd			fp,-16(sp)
	sd			ra,-8(sp)
	add			fp,sp,x0
	addi		sp,sp,-80
	jal			x0,.main_1
.main_1:
	addiw		t0,x0,1
	addi		t1,x0,0
	subw		t0,t1,t0
	fmv.w.x		ft0,t0
	fmv.w.x		ft1,x0
	feq.s		t0,ft0,ft1
	fmv.w.x		ft0,t0
	fmv.w.x		ft1,x0
	feq.s		t0,ft0,ft1
	fmv.w.x		ft0,t0
	fmv.w.x		ft1,x0
	feq.s		t0,ft0,ft1
	fmv.w.x		ft0,t0
	fmv.w.x		ft1,x0
	feq.s		t0,ft0,ft1
	fmv.w.x		ft0,t0
	fmv.w.x		ft1,x0
	feq.s		t0,ft0,ft1
	addi		t1,x0,0
	subw		t0,t1,t0
	addi		a0,t0,0
	call		putint
	addiw		t0,x0,10
	addi		a0,t0,0
	call		putch
	addiw		t0,x0,1
	addi		t1,x0,0
	subw		t0,t1,t0
	fmv.w.x		ft0,t0
	fmv.w.x		ft1,x0
	feq.s		t0,ft0,ft1
	fmv.w.x		ft0,t0
	fmv.w.x		ft1,x0
	feq.s		t0,ft0,ft1
	addi		a0,t0,0
	call		putint
	addiw		t0,x0,10
	addi		a0,t0,0
	call		putch
	addiw		t0,x0,0
	addw		a0,x0,t0
	addi		sp,sp,80
	ld			ra,-8(sp)
	ld			fp,-16(sp)
	jalr		x0,ra,0
	.data
