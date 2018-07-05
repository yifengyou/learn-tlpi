include ../Makefile.inc

GEN_EXE = orphan 

LINUX_EXE = \
	    demo_userns \
	    demo_uts_namespaces \
	    hostname \
	    multi_pidns \
	    ns_child_exec \
	    ns_exec \
	    ns_run \
	    pidns_init_sleep \
	    simple_init \
	    t_setns_userns \
	    unshare \
	    userns_child_exec \
	    userns_setns_test

EXE = ${GEN_EXE} ${LINUX_EXE}

all : ${EXE}

allgen : ${GEN_EXE}

clean : 
	${RM} ${EXE} *.o

showall :
	@ echo ${EXE}

demo_userns: demo_userns.o
	${CC} -o $@ demo_userns.o ${CFLAGS} ${LDLIBS} ${LINUX_LIBCAP}

t_setns_userns: t_setns_userns.o
	${CC} -o $@ t_setns_userns.o ${CFLAGS} ${LDLIBS} ${LINUX_LIBCAP}

unshare: unshare.o
	${CC} -o $@ unshare.o ${CFLAGS} ${LDLIBS} ${LINUX_LIBCAP}

userns_child_exec: userns_child_exec.o
	${CC} -o $@ userns_child_exec.o ${CFLAGS} ${LDLIBS} ${LINUX_LIBCAP}

userns_setns_test: userns_setns_test.o
	${CC} -o $@ userns_setns_test.o ${CFLAGS} ${LDLIBS} ${LINUX_LIBCAP}

${EXE} : ${TLPI_LIB}		# True as a rough approximation
