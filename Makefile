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

# Optional Node.js & WebAssembly bindings
NPM ?= npm

npm-build:
	@if command -v $(NPM) >/dev/null 2>&1; then \
		(cd npm && $(NPM) run build); \
	else \
		echo "Skipping npm build — $(NPM) not found"; \
	fi

npm-test: npm-build
	@if command -v $(NPM) >/dev/null 2>&1; then \
		(cd npm && $(NPM) test); \
	fi

