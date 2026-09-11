all := libqsys qsys_dlopen_test

LDLIBS-libqsys-Linux := -ldl
LDLIBS-libqsys-Darwin := -ldl
LDLIBS-libqsys-OpenBSD :=
LDLIBS-libqsys-FreeBSD :=
LDLIBS-libqsys-NetBSD :=
LDLIBS-libqsys-DragonFly :=
LDLIBS-qsys_dlopen_test := -lqsys

-include ../mk/include.mk

test: all
	LD_LIBRARY_PATH=./lib ./bin/qsys_dlopen_test
