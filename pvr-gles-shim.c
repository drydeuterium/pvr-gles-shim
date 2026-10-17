/*
 * pvr-gles-shim - work around an extension the PowerVR GLES driver advertises
 * but does not implement.
 *
 * Symptom: any GLES client that honours GL_EXT_texture_storage segfaults on the
 * first frame on this board (Orange Pi 4 Pro, PowerVR BXM-4-64, vendor stack
 * under /usr/local/lib/pvr-bxm). RetroArch 1.14 is one such client: gl2.c:330
 * calls glTexStorage2DEXT when gl_check_capability(GL_CAPS_TEX_STORAGE_EXT)
 * succeeds.
 *
 * Cause: the vendor driver lists GL_EXT_texture_storage in its extension string
 * but exports only the core ES 3.0 entry points. Neither it nor Mesa's
 * libGLESv2 exports glTexStorage2DEXT, so eglGetProcAddress("glTexStorage2DEXT")
 * returns NULL and the caller jumps to address 0. RetroArch does not test the
 * pointer, and on llvmpipe the capability check fails first, which is why the
 * same code path works there.
 *
 * Fix: answer the missing EXT entry point with the functionally equivalent core
 * function, which the driver does provide. Everything else is passed through
 * untouched.
 *
 * Build: gcc -shared -fPIC -O2 -o /usr/local/lib/pvr-gles-shim.so \
 *            /usr/local/src/pvr-gles-shim/pvr-gles-shim.c
 * Use:   LD_PRELOAD=/usr/local/lib/pvr-gles-shim.so <gles application>
 */

#define _GNU_SOURCE
#include <dlfcn.h>
#include <string.h>

typedef void *(*getproc_t)(const char *);

static getproc_t real_get_proc_address(void)
{
    static getproc_t real = NULL;
    if (!real)
        real = (getproc_t)dlsym(RTLD_NEXT, "eglGetProcAddress");
    return real;
}

/* Intercepted resolution path. Covers clients that look the name up at runtime,
 * which is what RetroArch does - it imports eglGetProcAddress but has no
 * glTexStorage2DEXT symbol of its own. */
void *eglGetProcAddress(const char *name)
{
    getproc_t real = real_get_proc_address();
    void *p;

    if (!real)
        return NULL;

    p = real(name);
    if (!p && name && strcmp(name, "glTexStorage2DEXT") == 0)
        p = real("glTexStorage2D");
    return p;
}

/* Also export the symbol directly, so a client that resolves through dlsym on
 * the global namespace is served as well. */
void glTexStorage2DEXT(unsigned int target, int levels, unsigned int internalformat,
                       int width, int height)
{
    typedef void (*core_t)(unsigned int, int, unsigned int, int, int);
    static core_t core = NULL;

    if (!core)
    {
        getproc_t real = real_get_proc_address();
        if (real)
            core = (core_t)real("glTexStorage2D");
    }
    if (core)
        core(target, levels, internalformat, width, height);
}
