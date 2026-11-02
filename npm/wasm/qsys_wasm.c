#include "../../src/qsys_wasm_kernel.c"

/* Backward compatibility alias */
ISO_EXPORT(uint64_t, qsys_wasm_version, void)
{
    return qsys_kernel_version();
}
