# Phase 29.4 — Final Root Cause Analysis
## Resolution of EGL Stall: ANGLE Worker Thread Pool Futex Timeout

### 1. Investigation Correction: LR vs PC
Phase 29.3's initial conclusion ("PID 417 waiting for allocator reply in ioctl") was based on reading the **Link Register (x30/LR)** — the return address from ioctl — rather than the actual **PC**. The correct `pt_regs` offset for PC is `kstack_page3 + 0xec0 + 0x100`; the earlier script read `kstack_page3 + 0xeb0 + 32*8 = kstack_page3 + 0xfb0` (= x30, 16 bytes short of the PC field).

### 2. Thread State Map at t = 20s (post CP-02 restore)

| PID | task_va | Syscall | PC | Library | State |
|-----|---------|---------|-----|---------|-------|
| 246 | `0xee51ec00` | ioctl (29) | `libc+0x70d78` | libc | binder receive, fd=3 |
| 410 | `0xee715e80` | ioctl (29) | `libc+0x70d78` | libc | binder receive, fd=3 |
| 411 | `0xee711b00` | ioctl (29) | `libc+0x70d78` | libc | binder receive, fd=3 |
| **417** | `0xfb178000` | **futex (98)** | `libc+0x128a0` | libc | `pthread_cond_timedwait`, condvar @ `0x7362f75b78` |
| 428 | `0xf3b90d80` | futex (98) | `libc+0x128a0` | libc | `pthread_cond_timedwait`, condvar @ `0x73d2f7e774` |
| 429 | `0xee715100` | futex (98) | `libc+0x128a0` | libc | `pthread_cond_timedwait`, condvar @ `0x7442f8a770` |
| 434 | `0xf1bc8d80` | futex↔ioctl | `libc+0x128a0`↔`+0x70d78` | libc | Coordinator: condvar wait ↔ hwbinder receive (fd=9) |

### 3. VMA Binary Search (mm→mm_rb O(log n))

| Address | Range | Offset | Mapping |
|---------|-------|--------|---------|
| `0x75732518a0` | `[0x757323f000, 0x75732d1000)` | `+0x128a0` | **libc.so** — `futex()` syscall stub |
| `0x75732afd78` | same | `+0x70d78` | **libc.so** — `ioctl()` syscall stub |
| `0x7573256184` | same | `+0x17184` | **libc.so** — LR inside `pthread_cond_timedwait` |
| `0x7362f75b78` | `[0x7362f74000, +256KB)` | `+0x1b78` | **anon** (ANGLE worker condvar) |
| `0x73d2f7e774` | `[0x73d2f79000, +256KB)` | `+0x5774` | **anon** (ANGLE worker condvar) |
| `0x7442f8a770` | `[0x7442f80000, +256KB)` | `+0xa770` | **anon** (ANGLE worker condvar) |

### 4. True Root Cause

```text
BA: eglInitialize()
  └─ ANGLE/Pastel EGL init
       ├─ Spawns worker pool: PIDs 417, 428, 429 (+ coordinator 434)
       │    Each worker: pthread_cond_timedwait(condvar_i, mutex)
       │                 libc+0x128a0 (futex syscall stub)
       │    Condvars in 256 KB anon mmaps (heap-allocated per-thread)
       │
       └─ Coordinator PID 434 oscillates:
            • condvar wait (libc futex) — waiting for GPU HAL response
            • ioctl(fd=9, hwbinder RECEIVE) — polling for HAL callbacks
            Neither ever arrives because QEMU has no GPU

QEMU: No hardware Qualcomm GPU
  → No /dev/kgsl-3d0 or equivalent
  → Vulkan/GPU HAL never responds to initialization request
  → pthread_cond_timedwait times out
  → ANGLE: Vulkan device enumeration failed
  → eglInitialize() returns EGL_FALSE

IAllocator PID 255: permanently idle (SYMPTOM, not cause)
  ← gralloc never called because eglCreateWindowSurface never reached
  ← Gate ordering: eglInitialize [FAIL] → eglCreateWindowSurface [never] → IAllocator [never]
```
