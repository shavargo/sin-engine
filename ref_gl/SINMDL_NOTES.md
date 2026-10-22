# SINMDL / GetRefAPI port notes (Phase 2)

REA dump: `C:\bws-development\sin-mp-rea\docs\ref-gl-decomp\`

| Symbol | Dump hint | Q2 anchor |
|--------|-----------|-----------|
| `GetRefAPI` | `GetRefAPI__0x10013e60.c` | `ref_gl/gl_rmain.c` |
| `R_Init` | string `ref_gl::R_Init()` | `R_Init` |
| `SINMDL_DrawModel` | `FUN_10019910` | **new** — not in stock Q2 |

Port approach: keep Q2 `GetRefAPI` export table; add SINMDL registration/draw beside MD2 path; feed from engine `SINMDL_*` in `sin/sinmdl.c` once engine cache exists (`sin-exe-decomp` SINMDL_* map).
