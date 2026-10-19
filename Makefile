all := libqsys qsys_dlopen_test qsys_port_test

LDLIBS-libqsys-Linux := -ldl
LDLIBS-libqsys-Darwin := -ldl
LDLIBS-libqsys-OpenBSD :=
LDLIBS-libqsys-FreeBSD :=
LDLIBS-libqsys-NetBSD :=
LDLIBS-libqsys-DragonFly :=
LDLIBS-libqsys-Windows := -lws2_32 -lpsapi
LDLIBS-qsys_dlopen_test := -lqsys
LDLIBS-qsys_dlopen_test-Windows := -lws2_32 -lpsapi
LDLIBS-qsys_port_test := -lqsys
LDLIBS-qsys_port_test-Windows := -lws2_32 -lpsapi

-include ../mk/include.mk

test: all
	PATH=./lib:$$PATH LD_LIBRARY_PATH=./lib ./bin/qsys_dlopen_test
	PATH=./lib:$$PATH LD_LIBRARY_PATH=./lib ./bin/qsys_port_test
