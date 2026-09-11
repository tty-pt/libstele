#include "./../include/ttypt/qsys.h"

#include <assert.h>
#include <stdio.h>

int
main(void)
{
	/* invalid path -> NULL + a non-NULL error string */
	void *bad = qsys_dlopen("/no/such/file/qsys_dlopen_test_missing.so", 0);
	assert(bad == NULL);
	assert(qsys_dlerror() != NULL);
	printf("ok: qsys_dlopen(invalid path) -> NULL + error\n");

	/* self-open: libqsys.so is on the loader path via -L./lib */
	void *h = qsys_dlopen("lib/libqsys.so", 0);
	assert(h != NULL);
	printf("ok: qsys_dlopen(lib/libqsys.so) -> handle\n");

	/* known exported symbol */
	void *sym = qsys_dlsym(h, "qsyslog_set");
	assert(sym != NULL);
	printf("ok: qsys_dlsym(qsyslog_set) -> non-NULL\n");

	/* unknown symbol */
	void *nosym = qsys_dlsym(h, "no_such_symbol_xyz");
	assert(nosym == NULL);
	printf("ok: qsys_dlsym(no_such_symbol_xyz) -> NULL\n");

	int rc = qsys_dlclose(h);
	assert(rc == 0);
	printf("ok: qsys_dlclose -> 0\n");

	/* QSYS_RTLD_NODELETE: flag accepted, handle still usable */
	void *h2 = qsys_dlopen("lib/libqsys.so", QSYS_RTLD_NODELETE);
	assert(h2 != NULL);
	assert(qsys_dlsym(h2, "qsyslog_set") != NULL);
	assert(qsys_dlclose(h2) == 0);
	printf("ok: qsys_dlopen(QSYS_RTLD_NODELETE) -> handle\n");

	printf("qsys_dlopen_test: all assertions passed\n");
	return 0;
}
