MUSL_FLAGS:=-I ./musl/include/ -L./musl/lib/
MUSL_DIR:=${CURDIR}/musl
RECORD_FLAGS:=-static -mno-avx
CFLAGS:=${MUSL_FLAGS} -g -O0 ${RECORD_FLAGS} \
  -Wall -Wextra \
  -Wno-unused-value -Wno-sign-compare -Wno-unused-parameter \
  -Wno-int-conversion -Wno-return-type
LDFLAGS:=${MUSL_FLAGS} -g -O0 -lc ${RECORD_FLAGS}
MUSL_BIN:=${MUSL_DIR}/bin/musl-gcc

fpir: export LD_BIND_NOW=1
fpir: ${MUSL_BIN} fpir.c
	${MUSL_BIN} ${CFLAGS} ${LDFLAGS} fpir.c -o $@

clean:
	rm -f fpir

clean_all: clean
	cd ${MUSL_DIR}; \
	make clean; \
	rm -rf bin

${MUSL_BIN}:
	cd ${MUSL_DIR}; \
	CFLAGS="${RECORD_FLAGS}" ./configure --prefix=${MUSL_DIR} \
	  --exec-prefix=${MUSL_DIR} --syslibdir=${MUSL_DIR} \
	  --disable-shared; \
	CFLAGS="${RECORD_FLAGS}" make; \
	CFLAGS="${RECORD_FLAGS}" make install


