all := libstele stele_dlopen_test stele_port_test
libstele-obj-y := src/stele_event.o src/stele_wq.o src/stele_proc.o src/stele_ipc.o

LDLIBS-libstele-Linux := -ldl -lpthread
LDLIBS-libstele-Darwin := -ldl -lpthread
LDLIBS-libstele-OpenBSD := -lpthread
LDLIBS-libstele-FreeBSD := -lpthread
LDLIBS-libstele-NetBSD := -lpthread
LDLIBS-libstele-DragonFly := -lpthread
LDLIBS-libstele-Windows := -lws2_32 -lpsapi -ladvapi32
LDLIBS-stele_dlopen_test := -lstele
LDLIBS-stele_dlopen_test-Windows := -lws2_32 -lpsapi -ladvapi32
LDLIBS-stele_port_test := -lstele
LDLIBS-stele_port_test-Windows := -lws2_32 -lpsapi -ladvapi32

WASM_SRCS := src/stele_wasm_kernel.c src/stele_event.c src/stele_wq.c

include ../mk/include.mk

wasm: ${wasmtarget}
	@cp -f ${wasmtarget} htdocs/stele.wasm 2>/dev/null || true

test: all
	PATH=./lib:$$PATH LD_LIBRARY_PATH=./lib ./bin/stele_dlopen_test
	PATH=./lib:$$PATH LD_LIBRARY_PATH=./lib ./bin/stele_port_test

# Optional Node.js & WebAssembly bindings
NPM ?= npm

npm-build:
	@cp -f include/ttypt/*.h npm/include/ttypt/
	@cp -f htdocs/stele-client.js npm/dist/client.js
	@if command -v $(NPM) >/dev/null 2>&1; then \
		(cd npm && $(NPM) run build); \
	else \
		echo "Skipping npm build — $(NPM) not found"; \
	fi

npm-test: npm-build
	@if command -v $(NPM) >/dev/null 2>&1; then \
		(cd npm && $(NPM) test); \
	fi
