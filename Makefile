all := libstele stele_dlopen_test stele_port_test
libstele-obj-y := src/stele_event.o

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

WASM_SRCS := src/stele_wasm_kernel.c src/stele_event.c
WASM_LDFLAGS := -mexec-model=reactor -Wl,--gc-sections -Wl,--no-entry -Wl,--allow-undefined -Wl,--export=malloc -Wl,--export=free -Wl,--initial-memory=16777216 -Wl,--max-memory=268435456

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
