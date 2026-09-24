# Phase 29.1 & 29.2 — Trace the BLAST / ANativeWindow Boundary & BootAnimation Lifecycle
## Observation-Only Trace Execution from CP-01

### Gate 29.1: Target Observation Ladder Results

During a 720.0-second native observation run from CP-01, the following counts were recorded:

| Boundary | ENTER | RETURN | Observation Status | Notes |
|---|---|---|---|---|
| `eglSwapBuffers` | 0 | - | **MISS** | BootAnimation render loop has not initiated buffer swaps |
| `ANativeWindow_queueBuffer` | 0 | - | **MISS** | Unreached (dependent on `eglSwapBuffers`) |
| `H2B queueBuffer candidate V1` | 0 | - | **MISS** | Unreached |
| `H2B queueBuffer candidate V2` | 0 | - | **MISS** | Unreached |
| `BpGraphicBufferProducer::queueBuffer` | 0 | - | **MISS** | Unreached |
| `HpGraphicBufferProducer::queueBuffer` | 0 | - | **MISS** | Unreached |
| `BLASTBufferQueue::onFrameAvailable` | 0 | - | **MISS** | Unreached |
| `acquireNextBufferLocked` | 0 | - | **MISS** | Unreached |
| `Transaction::setBuffer` | 0 | - | **MISS** | Unreached |
| `applyPendingTransactions` | 0 | - | **MISS** | Unreached |
| `Transaction::apply` | 0 | - | **MISS** | Unreached |
| `SurfaceFlinger::setTransactionState` | **157** | - | **HIT** | Actively handling system transactions (`LR=0x7a5bd25e34`) |

---

### Gate 29.2: BootAnimation Activation & Lifecycle from CP-01

#### The 7 Diagnostic Acceptance Questions Answered

| Question | Answer | Diagnostic Evidence |
| :--- | :--- | :--- |
| **1. Does BootAnimation still exist after CP-01 restore?** | **YES** | Kernel `task_struct` at `0xffffffc0ee51ec00` (PA `0x12e51ec00`), `comm='bootanimation'`. |
| **2. What PID/TID does it have?** | **PID 246, TGID 246, TID 246** | Single-threaded at CP-01. Thread group at `+0x560` points to self (no child threads yet). |
| **3. Is its main thread runnable, sleeping, or blocked?** | **RUNNABLE at CP-01, then polling IPC** | At CP-01: `TASK_RUNNING (0)` (yielded during `munmap`). Post-restore: active, executing `ioctl(BINDER_WRITE_READ)` + `usleep(100ms)`. |
| **4. What is its current PC?** | **`0x7577df0d78` (CP-01) $\to$ `0x75732afd78` (post-restore)** | CP-01: right after `svc #0` of `munmap` in `linker64`. Post-restore: `__ioctl` in `libc.so`. |
| **5. Which library is that PC inside?** | **`linker64` (CP-01) $\to$ `libc.so` (post-restore)** | Unmapping ELF padding (`linker64`) $\to$ calling `/dev/binder` driver (`libc.so`). |
| **6. Is it actually waiting on a synchronization primitive?** | **YES: Binder IPC to ServiceManager** | `ioctl(3, BINDER_WRITE_READ)` with `write_size=12`, `read_size=256`, waiting for service lookup reply. |
| **7. Does it ever execute BootAnimation code after restore?** | **YES: Enters `main()` at $t=25.3\text{s}$** | Successfully executed `_start` ($t=25.1\text{s}$) and `main()` ($t=25.3\text{s}$), stopping at the `checkService("SurfaceFlinger")` loop. |

#### Architectural Classification

$$\mathbf{Classification:\; B \; (BootAnimation\; started\; but\; blocked\; downstream)}$$

- **Nature of Block**: BootAnimation is alive, responsive, and executing normally. It is blocked waiting for an external IPC event: `servicemanager` answering `checkService("SurfaceFlinger") != nullptr`.
- **Root Blocker Identified**: BootAnimation cannot enter its rendering pipeline (`readyToRun()` $\to$ `threadLoop()` $\to$ `eglSwapBuffers()`) because it is gated by the availability of the `SurfaceFlinger` service registration in `servicemanager`.
