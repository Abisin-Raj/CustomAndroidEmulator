# Phase 29.3 — Trace BootAnimation Through EGL Initialization
## Trace Execution from CP-02 Checkpoint

### Milestone Ladder Verification Table

| Milestone | Target Symbol / Function | Address | Status | Result / Register State |
|---|---|---|:---:|---|
| **M0** | `getInternalDisplayToken` | `0x756f4ab310` | **HIT** | Valid display token `0xb4000072f2f9a540` |
| **M0.1**| `cbz x8` token check | `0x756f4ab34c` | **HIT** | Passed (`X8 = 0xb400007352f7c140`) |
| **M0.2**| `getActiveDisplayMode` call | `0x756f4ab388` | **HIT** | Called with display token |
| **M0.3**| `getActiveDisplayMode` return | `0x756f4ab390` | **HIT** | Status $0$ (`NO_ERROR`), $1080 \times 2340$ |
| **M0.4**| `createSurface("BootAnimation")` | `0x756f4ab624` | **HIT** | SurfaceControl constructed |
| **M0.5**| `Transaction` construction | `0x756f4ab6cc` | **HIT** | SurfaceComposerClient::Transaction ready |
| **M0.6**| `Transaction::apply()` call | `0x756f4aba34` | **HIT** | Applied to SurfaceFlinger via `/dev/binder` |
| **M0.7**| `Transaction::apply()` return | `0x756f4aba38` | **HIT** | Returned $0$ (`NO_ERROR`) in 3.15s |
| **M0.8**| `eglGetDisplay(EGL_DEFAULT_DISPLAY)` | `0x756f4aba4c` | **HIT** | Returned EGLDisplay `0xb4000073f2f7c840` |
| **M0.9**| `eglInitialize()` entry | `0x756f4aba5c` | **HIT** | Entered ANGLE/Pastel runtime |
| **M1** | `eglInitialize()` return | `0x756f4aba60` | **BLOCKED** | Stalled in `ioctl(9)` `/dev/hwbinder` |
| **M2** | `eglCreateWindowSurface()` | `0x756f4abab8` | 0 hits | Gated by `eglInitialize()` return |
| **M3** | `eglCreateContext()` | `0x756f4abae8` | 0 hits | Gated downstream |
| **M4** | `eglMakeCurrent()` | `0x756f4abb30` | 0 hits | Gated downstream |
| **M5** | `readyToRun()` return | `0x756f4abd74` | 0 hits | Gated downstream |
| **M6** | `BootAnimation::threadLoop()` | `0x756f4ad550` | 0 hits | Gated downstream |
| **M7** | `BootAnimation::movie()` | `0x756f4adb80` | 0 hits | Gated downstream |
| **M8** | `eglSwapBuffers()` | `0x756f4b05c0` | 0 hits | Gated downstream |

### Key Findings
1. BootAnimation advances past token query, display mode query (1080x2340), surface creation, and SurfaceFlinger transaction apply.
2. `eglInitialize()` is entered successfully via `libEGL.so`.
3. Execution does not return from `eglInitialize()`, preventing the thread from reaching `eglCreateWindowSurface()`, `eglMakeCurrent()`, or `threadLoop()`.
