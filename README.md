# pvr-gles-shim

Works around a PowerVR GLES driver that advertises an extension it does not
implement, which makes GLES clients segfault on the first frame.

Developed against the **Orange Pi 4 Pro** (Allwinner A733 / `sun60iw2`,
Imagination PowerVR B-Series BXM-4-64) running the vendor userspace stack from
Allwinner's `sun60iw2_packages` under `/usr/local/lib/pvr-bxm`.

## Symptom

Any GLES application that honours `GL_EXT_texture_storage` dies with `SIGSEGV`
as soon as it uploads its first texture. With RetroArch 1.14 it looks like this:

```
[INFO] [GL]: Vendor: Imagination Technologies, Renderer: PowerVR B-Series BXM-4-64.
[INFO] [GL]: Version: OpenGL ES 3.2 build 24.2@6603887.
[INFO] [GL]: Using 4 textures.
[INFO] [GL]: Loaded 1 program(s).
Segmentation fault
```

`gdb` puts the fault at address `0x0`:

```
#0  0x0000000000000000 in ?? ()
#1  gl2_load_texture_image (...) at gfx/drivers/gl2.c:330
#2  gl2_renderchain_init_texture_reference (...) at gl2.c:1647
#3  gl2_init_textures (...) at gl2.c:2277
```

## Root cause

`gl2.c:330` is the GLES branch of RetroArch's texture helper:

```c
if (gl_check_capability(GL_CAPS_TEX_STORAGE_EXT) && internalFormat != GL_BGRA_EXT)
{
#ifdef HAVE_OPENGLES2
   glTexStorage2DEXT(target, 1, internalFormat, width, height);   /* line 330 */
```

The driver reports `GL_EXT_texture_storage` in its extension string, so
RetroArch's capability check succeeds — but the driver exports only the core
ES 3.0 entry points:

```
glTexStorage2D
glTexStorage2DMultisample
glTexStorage3D
glTexStorage3DMultisample
```

The EXT alias is absent from the library, and

```c
eglGetProcAddress("glTexStorage2DEXT")  /* returns 0x0 */
```

RetroArch does not test the pointer before calling it. Nothing is wrong with the
kernel driver: `/sys/kernel/debug/pvr/status` reports `Driver Status: OK` with
zero server/HWR/CRR/SLR/WGP/TRP/FWF errors throughout.

### Why the same binary works on llvmpipe

Mesa's `libGLESv2.so.2` does not export `glTexStorage2DEXT` either — but it does
**not** advertise `GL_EXT_texture_storage`, because the functionality is core in
ES 3.0. So `gl_check_capability()` fails, the code takes the `glTexImage2D`
branch, and nothing crashes. The bug is therefore invisible on software
rendering and appears only on this vendor stack. That asymmetry is what makes it
worth a shim rather than a patch.

## Fix

Intercept `eglGetProcAddress` and answer the missing EXT entry point with the
functionally equivalent core function. Everything else is passed through
untouched. The symbol is also exported directly, so a client that resolves
through `dlsym` on the global namespace is served as well.

```sh
make
sudo make install          # /usr/local/lib/pvr-gles-shim.so
```

Use it per application — there is no need to make the vendor GL stack global:

```sh
LD_PRELOAD=/usr/local/lib/pvr-gles-shim.so \
LD_LIBRARY_PATH=/usr/local/lib/pvr-bxm:/usr/lib/aarch64-linux-gnu:/usr/lib \
LIBGL_DRIVERS_PATH=/usr/local/lib/pvr-bxm/dri \
retroarch
```

`LIBGL_DRIVERS_PATH` matters: without it Mesa-Loader looks in
`/usr/local/lib/dri`, fails to open `sunxi-drm_dri.so`, and the client falls back
to software rendering (or to a failing KMS path).

No `LD_PRELOAD` is needed on Mesa/llvmpipe, where the bug does not trigger. The
shim is inert there anyway: it only substitutes a name that would otherwise
resolve to `NULL`.

## Measured result

Orange Pi 4 Pro, RetroArch 1.14.0, same content, 20 s of wall clock, CPU time
consumed by the whole process tree:

| Content | Mesa llvmpipe | PowerVR BXM + shim |
|---|---|---|
| RetroArch menu | 73.8 s | **10.5 s** |
| `invaders.nes` (nestopia core) | 47.7 s | **11.9 s** |

Roughly 4x to 7x less CPU for identical output, with
`Renderer: PowerVR B-Series BXM-4-64` and real GPU activity in
`/sys/kernel/debug/pvr/status` (`3D: 2 %`, `Server Errors: 0`).

## Scope and caveats

- This is a workaround for a driver/vendor-stack defect, not a general fix. If
  Allwinner or Imagination ships a GLES library that exports
  `glTexStorage2DEXT`, the shim becomes unnecessary.
- It only makes sense on this hardware and this driver. Loading it elsewhere is
  pointless but harmless.
- Other extensions in the same driver may be advertised without an
  implementation. If a different GLES client crashes at address `0x0`, check
  whether the driver claims an extension it does not export — the same
  pass-through shim shape will confirm it quickly.
- The name of the affected product and driver version are recorded above because
  the behaviour is version-specific.

## License

MIT. See `LICENSE`.
