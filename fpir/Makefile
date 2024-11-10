MUSL_FLAGS:=-I ./musl/include/ -L./musl/lib/
MUSL_DIR:=${CURDIR}/musl
MUSL_BIN:=${MUSL_DIR}/bin/musl-gcc

SHARED_CFLAGS:=-g -O0 \
  -Wall -Wextra \
  -Wno-unused-value -Wno-sign-compare -Wno-unused-parameter \
  -Wno-int-conversion -Wno-return-type
SHARED_LDFLAGS:=-g -O0

RECORD_FLAGS:=-static -mno-avx
CFLAGS:=${MUSL_FLAGS} ${SHARED_CFLAGS} ${RECORD_FLAGS}
LDFLAGS:=${MUSL_FLAGS} ${SHARED_LDFLAGS} -lc ${RECORD_FLAGS}

RISCV_FLAGS:=-mcmodel=medany
MUSL_RISCV_BUILD_FLAGS:=""
MUSL_RISCV_FLAGS:="" # -I ./musl-riscv/include/ -L./musl-riscv/lib/
MUSL_RISCV_DIR:=${CURDIR}/musl-cross-make
MUSL_RISCV_GCC:=${MUSL_RISCV_DIR}/output/bin/riscv64-linux-musl-gcc
MUSL_RISCV_OBJCOPY:=${MUSL_RISCV_DIR}/output/bin/riscv64-linux-musl-objcopy

BM_CFLAGS:=${SHARED_CFLAGS} -ffreestanding -static -nostdlib ${RISCV_FLAGS}
BM_LDFLAGS:=${SHARED_LDFLAGS} -ffreestanding -static -nostdlib ${RISCV_FLAGS}

QEMU_RISCV_FLAGS:= -machine virt -smp 1 -m 128M \
  -bios none -nographic \
  -global virtio-mmio.force-legacy=false
QEMU_RISCV_DEBUG_FLAGS:=-S -s

fpir: export LD_BIND_NOW=1
fpir: ${MUSL_BIN} fpir.c
	${MUSL_BIN} ${CFLAGS} ${LDFLAGS} fpir.c -o $@

fpir_bm: export LD_BIND_NOW=1
fpir_bm: ${MUSL_RISCV_GCC} fpir.c riscv.c riscv.h riscv.ld riscv.s riscv_kernel.o
	${MUSL_RISCV_GCC} -Triscv.ld \
		${BM_CFLAGS} \
		${BM_LDFLAGS} \
		fpir.c riscv.c riscv.s riscv_kernel.o \
		-o $@

riscv_kernel.o: riscv-kernel.fp ${MUSL_RISCV_OBJCOPY}
	${MUSL_RISCV_OBJCOPY} --input binary \
		--output elf64-littleriscv \
		--binary-architecture riscv \
		--rename-section .data=.kernel,CONTENTS,ALLOC,LOAD,READONLY,DATA \
		$< $@

qemu_riscv: fpir_bm
	qemu-system-riscv64 ${QEMU_RISCV_FLAGS} -kernel fpir_bm
qemu_riscv_debug: fpir_bm
	qemu-system-riscv64 ${QEMU_RISCV_FLAGS} ${QEMU_RISCV_DEBUG_FLAGS} -kernel fpir_bm

clean:
	rm -f fpir riscv_kernel.o fpir_bm

clean_all: clean
	cd ${MUSL_DIR}; \
	make clean; \
	rm -rf bin
	cd ${MUSL_RISCV_DIR}; \
	make clean; \
	rm -rf output

${MUSL_BIN}:
	cd ${MUSL_DIR}; \
	CFLAGS="${RECORD_FLAGS}" ./configure --prefix=${MUSL_DIR} \
	  --exec-prefix=${MUSL_DIR} --syslibdir=${MUSL_DIR} \
	  --disable-shared; \
	CFLAGS="${RECORD_FLAGS}" make; \
	CFLAGS="${RECORD_FLAGS}" make install

${MUSL_RISCV_OBJCOPY}:
${MUSL_RISCV_GCC}:
	cd ${MUSL_RISCV_DIR}; \
	echo -e "TARGET = riscv64-linux-musl\nCOMMON_CONFIG += ${MUSL_RISCV_BUILD_FLAGS}" > config.mak; \
	make install

