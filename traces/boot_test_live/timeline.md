# Boot Trace Session: boot-test-live

- Total Events: 5
- Duration: 2.14s

## Execution Event Log

| Timestamp (s) | CPU | Type | PC | Symbol |
|---|---|---|---|---|
| 2.143 | vCPU0 | EXECUTION | 0xFFFFFF8008080000 | start_kernel |
| 2.143 | vCPU0 | SYMBOL_ENTRY | 0xFFFFFF80080DC200 | do_initcalls |
| 2.143 | vCPU1 | CPU_ONLINE | 0xFFFFFF800817A000 | secondary_start_kernel |
| 2.143 | vCPU0 | WATCHPOINT | 0xFFFFFF80080DC214 | do_initcalls |
| 2.143 | vCPU0 | BLOCKER_DETECTED | 0xFFFFFF80080DC230 | do_initcalls |
