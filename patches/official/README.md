# Official Patch Files

This folder contains the original vendor patch sources:

- `Patch.c`
- `Patches.h`

Purpose:
- Keep the official patch files available for users/reference.
- Keep them outside `src/` so they are not compiled into the library build.

Library runtime uses:
- `src/TEF668XPatchData.cpp`

That file contains the patch/LUT byte data used by `TEF668X::initHard()`.
