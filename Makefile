all := libstele stele_dlopen_test stele_port_test

LDLIBS-libstele-Linux := -ldl
LDLIBS-libstele-Darwin := -ldl
LDLIBS-libstele-OpenBSD :=
LDLIBS-libstele-FreeBSD :=
LDLIBS-libstele-NetBSD :=
LDLIBS-libstele-DragonFly :=
LDLIBS-libstele-Windows := -lws2_32 -lpsapi
LDLIBS-stele_dlopen_test := -lstele
LDLIBS-stele_dlopen_test-Windows := -lws2_32 -lpsapi
LDLIBS-stele_port_test := -lstele
LDLIBS-stele_port_test-Windows := -lws2_32 -lpsapi

-include ../mk/include.mk

test: all
	PATH=./lib:$$PATH LD_LIBRARY_PATH=./lib ./bin/stele_dlopen_test
	PATH=./lib:$$PATH LD_LIBRARY_PATH=./lib ./bin/stele_port_test

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
