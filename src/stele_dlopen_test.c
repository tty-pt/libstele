#include "./../include/ttypt/stele.h"

#include <assert.h>
#include <stdio.h>

#ifdef _WIN32
#define LIBSTELE_PATH "lib/libstele.dll"
#else
#define LIBSTELE_PATH "lib/libstele.so"
#endif

int
main(void)
{
	/* invalid path -> NULL + a non-NULL error string */
	void *bad = stele_dlopen("/no/such/file/stele_dlopen_test_missing.so", 0);
	assert(bad == NULL);
	assert(stele_dlerror() != NULL);
	printf("ok: stele_dlopen(invalid path) -> NULL + error\n");

	/* self-open: libstele is on the loader path via -L./lib */
	void *h = stele_dlopen(LIBSTELE_PATH, 0);
	assert(h != NULL);
	printf("ok: stele_dlopen(%s) -> handle\n", LIBSTELE_PATH);

	/* known exported symbol */
	void *sym = stele_dlsym(h, "stllog_set");
	assert(sym != NULL);
	printf("ok: stele_dlsym(stllog_set) -> non-NULL\n");

	/* unknown symbol */
	void *nosym = stele_dlsym(h, "no_such_symbol_xyz");
	assert(nosym == NULL);
	printf("ok: stele_dlsym(no_such_symbol_xyz) -> NULL\n");

	int rc = stele_dlclose(h);
	assert(rc == 0);
	printf("ok: stele_dlclose -> 0\n");

	printf("stele_dlopen_test: all assertions passed\n");
	return 0;
}
