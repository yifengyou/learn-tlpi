include ../Makefile.inc

GEN_EXE = 

LINUX_EXE = cap_text check_password_caps

EXE = ${GEN_EXE} ${LINUX_EXE}

all : ${EXE}

allgen : ${GEN_EXE}

LDLIBS = ${IMPL_LDLIBS} ${LINUX_LIBCAP} ${LINUX_LIBCRYPT}
        # The only program we build here needs libcap and libcrypt

clean : 
	${RM} ${EXE} *.o

showall :
	@ echo ${EXE}

${EXE} : ${TLPI_LIB}		# True as a rough approximation
