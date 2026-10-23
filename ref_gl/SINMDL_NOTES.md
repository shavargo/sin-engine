# SINMDL / GetRefAPI port notes (Phase 2)

| Symbol | Notes | Q2 anchor |
|--------|-------|-----------|
| `GetRefAPI` | SiN ref_gl export table | `ref_gl/gl_rmain.c` |
| `R_Init` | string `ref_gl::R_Init()` | `R_Init` |
| `SINMDL_DrawModel` | SINMDL mesh draw | `ref_gl/gl_sinmdl.c` |

Port approach: keep Q2 `GetRefAPI` export table; add SINMDL registration/draw beside MD2 path; feed from engine `SINMDL_*` in `sin/sinmdl.c`.
