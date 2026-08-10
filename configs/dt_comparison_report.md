# Analysis Report: DevTools v1.1 Tool 3 - Device Tree Analyzer & Comparison Report
**Execution Status**: SUCCESS

## Executive Summary
Successfully validated DevTools v1.1 DeviceTreeAnalyzer & DeviceTreeComparator capabilities. Parsed target vendor DTS, compared against QEMU virt DT specification, extracted missing hardware nodes (qcom,gcc-sm6150), MMIO base (0x00100000), region size (0x001F0000), and dependencies.

## Environment details
- **Target DTS**: kernel_extracted.dts
- **QEMU Spec**: virt.dtb
- **Missing Nodes Count**: 278
- **Missing Compatibles**: 278

## Register Dump
```text
=== CPU 0 Register Snapshot ===
  X0 =0x0000000000000000  X1 =0x0000000000000000  X2 =0x0000000000000000  X3 =0x0000000000000000
  X4 =0x0000000000000000  X5 =0x0000000000000000  X6 =0x0000000000000000  X7 =0x0000000000000000
  X8 =0x0000000000000000  X9 =0x0000000000000000  X10=0x0000000000000000  X11=0x0000000000000000
  X12=0x0000000000000000  X13=0x0000000000000000  X14=0x0000000000000000  X15=0x0000000000000000
  X16=0x0000000000000000  X17=0x0000000000000000  X18=0x0000000000000000  X19=0x0000000000000000
  X20=0x0000000000000000  X21=0x0000000000000000  X22=0x0000000000000000  X23=0x0000000000000000
  X24=0x0000000000000000  X25=0x0000000000000000  X26=0x0000000000000000  X27=0x0000000000000000
  X28=0x0000000000000000  X29=0x0000000000000000  X30=0x0000000000000000
  SP =0x0000000000000000   PC =0x0000000040000000   LR =0x0000000000000000
  PSTATE=0x00000000  CurrentEL=0
--- System EL1 Control Registers ---
  SCTLR =0x0000000000000000   TTBR0=0x0000000000000000
  TTBR1 =0x0000000000000000   TCR  =0x0000000000000000
  MAIR  =0x0000000000000000   ESR  =0x0000000000000000   FAR=0x0000000000000000
```

## Disassembly Context
```text
--- Device Tree Comparison Matrix ---
# DEVICE TREE COMPATIBILITY & HARDWARE CONTRACT REPORT
**Target DTS**: `kernel_extracted.dts` | **QEMU DTB**: `virt.dtb (QEMU default)`

### Status: DRIVER PROBE IMPOSSIBLE (Missing Vendor Hardware Contract)

## Missing Target Hardware Devices

### Device Node: `cpu@700`
- **Compatible String**: `arm,armv8`
- **MMIO Region**: Base `0x00000000` | Size `0x00000700` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `l1-dcache`
- **Compatible String**: `arm,arch-cache`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `soc`
- **Compatible String**: `simple-bus`
- **MMIO Region**: Base `0x17A00000` | Size `0x00010000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `interrupt-controller@b220000`
- **Compatible String**: `qcom,pdc-sm6150`
- **MMIO Region**: Base `0x0B220000` | Size `0x00000400` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,memshare`
- **Compatible String**: `qcom,memshare`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,client_3`
- **Compatible String**: `qcom,memshare-peripheral`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `timer@0x17c20000`
- **Compatible String**: `arm,armv7-timer-mem`
- **MMIO Region**: Base `0x17C20000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `scc_pll_out_aux2`
- **Compatible String**: `fixed-clock`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `scc_pll_out_aux`
- **Compatible String**: `fixed-factor-clock`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,rpmhclk`
- **Compatible String**: `qcom,rpmh-clk-sm6150`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,aopclk`
- **Compatible String**: `qcom,aop-qmp-clk`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,gcc@100000`
- **Compatible String**: `qcom,gcc-sm6150, syscon`
- **MMIO Region**: Base `0x00100000` | Size `0x001F0000` bytes
- **Power Supplies**: `vdd_cx-supply, vdd_cx_ao-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `syscon@5091540`
- **Compatible String**: `syscon`
- **MMIO Region**: Base `0x05091540` | Size `0x00000004` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,videocc@ab00000`
- **Compatible String**: `qcom,videocc-sm6150, syscon`
- **MMIO Region**: Base `0x0AB00000` | Size `0x00010000` bytes
- **Power Supplies**: `vdd_cx-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,camcc@ad00000`
- **Compatible String**: `qcom,camcc-sm6150, syscon`
- **MMIO Region**: Base `0x0AD00000` | Size `0x00010000` bytes
- **Power Supplies**: `vdd_cx-supply, vdd_mx-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,dispcc@af00000`
- **Compatible String**: `qcom,dispcc-sm6150, syscon`
- **MMIO Region**: Base `0x0AF00000` | Size `0x00020000` bytes
- **Power Supplies**: `vdd_cx-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,gpupcc@5090000`
- **Compatible String**: `qcom,gpucc-sm6150, syscon`
- **MMIO Region**: Base `0x05090000` | Size `0x00009000` bytes
- **Power Supplies**: `vdd_cx-supply, vdd_mx-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,scc@62b10000`
- **Compatible String**: `qcom,scc-sm6150`
- **MMIO Region**: Base `0x62B10000` | Size `0x00030000` bytes
- **Power Supplies**: `vdd_scc_cx-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cpucc@18321000`
- **Compatible String**: `qcom,clk-cpu-osm-sm6150`
- **MMIO Region**: Base `0x18321000` | Size `0x00001400` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,limits-dcvs@18350800`
- **Compatible String**: `qcom,msm-hw-limits`
- **MMIO Region**: Base `0x18350800` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cc-debug`
- **Compatible String**: `qcom,debugcc-sm6150`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `cpu-pmu`
- **Compatible String**: `arm,armv8-pmuv3`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `dsu_pmu@0`
- **Compatible String**: `arm,dsu-pmu`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-imem@146aa000`
- **Compatible String**: `qcom,msm-imem`
- **MMIO Region**: Base `0x146AA000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `restart_reason@65c`
- **Compatible String**: `qcom,msm-imem-restart_reason`
- **MMIO Region**: Base `0x0000065C` | Size `0x00000004` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `dload_type@1c`
- **Compatible String**: `qcom,msm-imem-dload-type`
- **MMIO Region**: Base `0x0000001C` | Size `0x00000004` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `boot_stats@6b0`
- **Compatible String**: `qcom,msm-imem-boot_stats`
- **MMIO Region**: Base `0x000006B0` | Size `0x00000020` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `kaslr_offset@6d0`
- **Compatible String**: `qcom,msm-imem-kaslr_offset`
- **MMIO Region**: Base `0x000006D0` | Size `0x0000000C` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `pil@94c`
- **Compatible String**: `qcom,msm-imem-pil`
- **MMIO Region**: Base `0x0000094C` | Size `0x000000C8` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `diag_dload@c8`
- **Compatible String**: `qcom,msm-imem-diag-dload`
- **MMIO Region**: Base `0x000000C8` | Size `0x000000C8` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `restart@c264000`
- **Compatible String**: `qcom,pshold`
- **MMIO Region**: Base `0x0C264000` | Size `0x00000004` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,mpm2-sleep-counter@0xc221000`
- **Compatible String**: `qcom,mpm2-sleep-counter`
- **MMIO Region**: Base `0x0C221000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,sps`
- **Compatible String**: `qcom,msm-sps-4k`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,gpi-dma@0xa00000`
- **Compatible String**: `qcom,gpi-dma`
- **MMIO Region**: Base `0x00A00000` | Size `0x00060000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `aop-msg-client`
- **Compatible String**: `qcom,debugfs-qmp-client`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-rtb`
- **Compatible String**: `qcom,msm-rtb`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,wdt@17c10000`
- **Compatible String**: `qcom,msm-watchdog`
- **MMIO Region**: Base `0x17C10000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,chd_gold`
- **Compatible String**: `qcom,core-hang-detect`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `kryo-erp`
- **Compatible String**: `arm,arm64-kryo-cpu-erp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,ghd`
- **Compatible String**: `qcom,gladiator-hang-detect-v3`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `cpuss_dump`
- **Compatible String**: `qcom,cpuss-dump`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `mem_dump`
- **Compatible String**: `qcom,mem-dump`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `mailbox@af20000`
- **Compatible String**: `qcom,tcs-drv`
- **MMIO Region**: Base `0x0AF20000` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `system_pm`
- **Compatible String**: `qcom,system-pm`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cmd-db@c3f000c`
- **Compatible String**: `qcom,cmd-db`
- **MMIO Region**: Base `0x0C3F000C` | Size `0x00000008` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `dcc_v2@10a2000`
- **Compatible String**: `qcom,dcc-v2`
- **MMIO Region**: Base `0x010A2000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,llcc@9200000`
- **Compatible String**: `qcom,llcc-core, simple-mfd`
- **MMIO Region**: Base `0x09200000` | Size `0x00450000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,llcc-perfmon`
- **Compatible String**: `qcom,llcc-perfmon`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,llcc-erp`
- **Compatible String**: `qcom,llcc-erp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,llcc-amon`
- **Compatible String**: `qcom,llcc-amon`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `ufsice@1d90000`
- **Compatible String**: `qcom,ice`
- **MMIO Region**: Base `0x01D90000` | Size `0x00008000` bytes
- **Power Supplies**: `vdd-hba-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `sdhci@8804000`
- **Compatible String**: `qcom,sdhci-msm-v5`
- **MMIO Region**: Base `0x08804000` | Size `0x00001000` bytes
- **Power Supplies**: `vdd-supply, vdd-io-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qseecom@86d00000`
- **Compatible String**: `qcom,qseecom`
- **MMIO Region**: Base `0x86D00000` | Size `0x02200000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `smcinvoke@86d00000`
- **Compatible String**: `qcom,smcinvoke`
- **MMIO Region**: Base `0x86D00000` | Size `0x02200000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qrng@793000`
- **Compatible String**: `qcom,msm-rng`
- **MMIO Region**: Base `0x00793000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `ufsphy_mem@1d87000`
- **Compatible String**: `qcom,ufs-phy-qmp-v3-660`
- **MMIO Region**: Base `0x01D87000` | Size `0x00000DB8` bytes
- **Power Supplies**: `vdda-phy-supply, vdda-pll-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `ufshc@1d84000`
- **Compatible String**: `qcom,ufshc`
- **MMIO Region**: Base `0x01D84000` | Size `0x00003000` bytes
- **Power Supplies**: `vdd-hba-supply, vcc-supply, vccq2-supply, vddp-ref-clk-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcedev@1de0000`
- **Compatible String**: `qcom,qcedev`
- **MMIO Region**: Base `0x01DE0000` | Size `0x00020000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm_hdcp`
- **Compatible String**: `qcom,msm-hdcp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcrypto@1de0000`
- **Compatible String**: `qcom,qcrypto`
- **MMIO Region**: Base `0x01DE0000` | Size `0x00020000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `tz-log@146aa720`
- **Compatible String**: `qcom,tz-log`
- **MMIO Region**: Base `0x146AA720` | Size `0x00003000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,spmi@c440000`
- **Compatible String**: `qcom,spmi-pmic-arb`
- **MMIO Region**: Base `0x0C440000` | Size `0x00001100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,power-on@800`
- **Compatible String**: `qcom,qpnp-power-on`
- **MMIO Region**: Base `0x00000800` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `vadc@3100`
- **Compatible String**: `qcom,spmi-adc5`
- **MMIO Region**: Base `0x00003100` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `adc_tm@3500`
- **Compatible String**: `qcom,adc-tm5`
- **MMIO Region**: Base `0x00003500` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,misc@900`
- **Compatible String**: `qcom,qpnp-misc`
- **MMIO Region**: Base `0x00000900` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,qpnp-smb5`
- **Compatible String**: `qcom,qpnp-smb5`
- **MMIO Region**: Base `0x00001000` | Size `0x00000100` bytes
- **Power Supplies**: `dpdm-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,usb-pdphy@1700`
- **Compatible String**: `qcom,qpnp-pdphy`
- **MMIO Region**: Base `0x00001700` | Size `0x00000100` bytes
- **Power Supplies**: `vdd-pdphy-supply, vbus-supply, vconn-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qpnp,qg`
- **Compatible String**: `qcom,qpnp-qg`
- **MMIO Region**: Base `0x00004800` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `bcl@3d00`
- **Compatible String**: `qcom,bcl-v5`
- **MMIO Region**: Base `0x00003D00` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `bcl-soc`
- **Compatible String**: `qcom,msm-bcl-soc`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,temp-alarm@2400`
- **Compatible String**: `qcom,spmi-temp-alarm`
- **MMIO Region**: Base `0x00002400` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `clock-controller@5b00`
- **Compatible String**: `qcom,spmi-clkdiv`
- **MMIO Region**: Base `0x00005B00` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `pinctrl@c000`
- **Compatible String**: `qcom,spmi-gpio`
- **MMIO Region**: Base `0x0000C000` | Size `0x00000C00` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,pm6150_rtc`
- **Compatible String**: `qcom,qpnp-rtc`
- **MMIO Region**: Base `0x00006000` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,pm6150l@5`
- **Compatible String**: `qcom,spmi-pmic`
- **MMIO Region**: Base `0x00000005` | Size `0x00000000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,lcdb@ec00`
- **Compatible String**: `qcom,qpnp-lcdb-regulator`
- **MMIO Region**: Base `0x0000EC00` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,leds@d300`
- **Compatible String**: `qcom,qpnp-flash-led-v2`
- **MMIO Region**: Base `0x0000D300` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,wled@d800`
- **Compatible String**: `qcom,pm6150l-spmi-wled`
- **MMIO Region**: Base `0x0000D800` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,pwms@b100`
- **Compatible String**: `qcom,pwm-lpg`
- **MMIO Region**: Base `0x0000B100` | Size `0x00000300` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,leds@d000`
- **Compatible String**: `qcom,tri-led`
- **MMIO Region**: Base `0x0000D000` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,amoled`
- **Compatible String**: `qcom,qpnp-amoled-regulator`
- **MMIO Region**: Base `0x0000E000` | Size `0x00000100` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `bt_wcn3990`
- **Compatible String**: `qca,wcn3990`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `bt-vdd-core-supply, bt-vdd-pa-supply, bt-vdd-ldo-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-eud@88e0000`
- **Compatible String**: `qcom,msm-eud`
- **MMIO Region**: Base `0x088E0000` | Size `0x00002000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `slim@62e40000`
- **Compatible String**: `qcom,slim-ngd`
- **MMIO Region**: Base `0x62E40000` | Size `0x0002C000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `msm_dai_slim`
- **Compatible String**: `qcom,msm-dai-slim`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `tavil_codec`
- **Compatible String**: `qcom,tavil-slim-pgd`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `cdc-vdd-buck-supply, cdc-buck-sido-supply, cdc-vdd-tx-h-supply, cdc-vdd-rx-h-supply, cdc-vddpx-1-supply, cdc-vdd-mic-bias-supply, vreg-micb-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `wcn3990`
- **Compatible String**: `qcom,btfmslim_slave`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `hwlock`
- **Compatible String**: `qcom,tcsr-mutex`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smem`
- **Compatible String**: `qcom,smem`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `mailbox@17c00000`
- **Compatible String**: `qcom,sm8150-apcs-hmss-global`
- **MMIO Region**: Base `0x17C00000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-cdsp-loader`
- **Compatible String**: `qcom,cdsp-loader`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-adsprpc-mem`
- **Compatible String**: `qcom,msm-adsprpc-mem-region`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm_fastrpc`
- **Compatible String**: `qcom,msm-fastrpc-compute`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm_fastrpc_compute_cb13`
- **Compatible String**: `qcom,msm-fastrpc-compute-cb`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,glink`
- **Compatible String**: `qcom,glink`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm_fastrpc_rpmsg`
- **Compatible String**: `qcom,msm-fastrpc-rpmsg`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm_cdsprm_rpmsg`
- **Compatible String**: `qcom,msm-cdsprm-rpmsg`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm_cdsp_rm`
- **Compatible String**: `qcom,msm-cdsp-rm`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm_hvx_rm`
- **Compatible String**: `qcom,msm-hvx-rm`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,glinkpkt`
- **Compatible String**: `qcom,glinkpkt`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,qmp-aop@c300000`
- **Compatible String**: `qcom,qmp-mbox`
- **MMIO Region**: Base `0x0C300000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smp2p_sleepstate`
- **Compatible String**: `qcom,smp2p-sleepstate`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smp2p-cdsp`
- **Compatible String**: `qcom,smp2p`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `tsens@c222000`
- **Compatible String**: `qcom,sm6150-tsens`
- **MMIO Region**: Base `0x0C222000` | Size `0x00000008` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,kgsl-hyp`
- **Compatible String**: `qcom,pil-tz-generic`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm_gsi`
- **Compatible String**: `qcom,msm_gsi`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,rmnet-ipa`
- **Compatible String**: `qcom,rmnet-ipa3`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,ipa@1e00000`
- **Compatible String**: `qcom,ipa`
- **MMIO Region**: Base `0x01E00000` | Size `0x00034000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smp2p_map_ipa_1_in`
- **Compatible String**: `qcom,smp2p-map-ipa-1-in`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `ipa_smmu_ap`
- **Compatible String**: `qcom,ipa-smmu-ap-cb`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `ipa_smmu_wlan`
- **Compatible String**: `qcom,ipa-smmu-wlan-cb`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `ipa_smmu_uc`
- **Compatible String**: `qcom,ipa-smmu-uc-cb`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,icnss@18800000`
- **Compatible String**: `qcom,icnss`
- **MMIO Region**: Base `0x18800000` | Size `0x00800000` bytes
- **Power Supplies**: `vdd-cx-mx-supply, 8-xo-supply, 3-rfa-supply, 3-ch0-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,rmtfs_sharedmem@0`
- **Compatible String**: `qcom,sharedmem-uio`
- **MMIO Region**: Base `0x00000000` | Size `0x00200000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `llcc-pmu@90cc000`
- **Compatible String**: `qcom,qcom-llcc-pmu`
- **MMIO Region**: Base `0x090CC000` | Size `0x00000300` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `keepalive-opp-table`
- **Compatible String**: `operating-points-v2`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,gpubw`
- **Compatible String**: `qcom,devbw`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cpu-cpu-llcc-bwmon@90b6400`
- **Compatible String**: `qcom,bimc-bwmon4`
- **MMIO Region**: Base `0x090B6300` | Size `0x00000300` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cpu-llcc-ddr-bwmon@90cd000`
- **Compatible String**: `qcom,bimc-bwmon5`
- **MMIO Region**: Base `0x090CD000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cpu6-cpu-l3-lat`
- **Compatible String**: `devfreq-simple-dev`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cpu6-llcc-ddr-latmon`
- **Compatible String**: `qcom,arm-memlat-mon`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cpu6-computemon`
- **Compatible String**: `qcom,arm-cpu-mon`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,bus_proxy_client`
- **Compatible String**: `qcom,bus-proxy-client`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `cx_ipeak@01fed000`
- **Compatible String**: `qcom,cx-ipeak-v1`
- **MMIO Region**: Base `0x01FED000` | Size `0x00000028` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `demux`
- **Compatible String**: `qcom,demux`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `pinctrl@03000000`
- **Compatible String**: `qcom,sm6150-pinctrl`
- **MMIO Region**: Base `0x03000000` | Size `0x00DC2000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `slpi_pinctrl@62B40000`
- **Compatible String**: `qcom,slpi-pinctrl`
- **MMIO Region**: Base `0x62B40000` | Size `0x00020000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `rpmh-regulator-lcxlvl`
- **Compatible String**: `qcom,rpmh-arc-regulator`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `mx-cdev-lvl`
- **Compatible String**: `qcom,regulator-cooling-device`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `regulator-cdev-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `regulator-cdev`
- **Compatible String**: `qcom,rpmh-reg-cdev`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `rpmh-regulator-bobc1`
- **Compatible String**: `qcom,rpmh-vrm-regulator`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,lpm-levels`
- **Compatible String**: `qcom,lpm-levels`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,rpm-stats@c300000`
- **Compatible String**: `qcom,rpm-stats`
- **MMIO Region**: Base `0x0C300000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,rpmh-master-stats@b221200`
- **Compatible String**: `qcom,rpmh-master-stats-v1`
- **MMIO Region**: Base `0x0B221200` | Size `0x00000060` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,gdsc@ab00814`
- **Compatible String**: `qcom,gdsc`
- **MMIO Region**: Base `0x0AB00814` | Size `0x00000004` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,qupv3_2_geni_se@626c0000`
- **Compatible String**: `qcom,qupv3-geni-se`
- **MMIO Region**: Base `0x626C0000` | Size `0x00006000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,qup_uart@0x880000`
- **Compatible String**: `qcom,msm-geni-console`
- **MMIO Region**: Base `0x00880000` | Size `0x00004000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `i2c@62694000`
- **Compatible String**: `qcom,i2c-geni`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `novatek@62`
- **Compatible String**: `novatek,NVT-ts`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `novatek-mp-criteria-5916@0`
- **Compatible String**: `novatek-mp-criteria-5916`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `focaltech@38`
- **Compatible String**: `focaltech,fts`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smb1355@c`
- **Compatible String**: `qcom,i2c-pmic`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,charge_pump`
- **Compatible String**: `qcom,smb1390-charger-psy`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smb1355-charger@1000`
- **Compatible String**: `qcom,smb1355`
- **MMIO Region**: Base `0x00001000` | Size `0x00000700` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `spi@62688000`
- **Compatible String**: `qcom,spi-geni`
- **MMIO Region**: Base `0x62688000` | Size `0x00004000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,qup_uart@0xa8c000`
- **Compatible String**: `qcom,msm-geni-serial-hs`
- **MMIO Region**: Base `0x00A8C000` | Size `0x00004000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qmi-tmd-devices`
- **Compatible String**: `qcom,qmi-cooling-devices`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `cxip-cdev@1fed000`
- **Compatible String**: `qcom,cxip-lm-cooling-device`
- **MMIO Region**: Base `0x01FED000` | Size `0x00000024` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,kgsl-busmon`
- **Compatible String**: `qcom,kgsl-busmon`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,kgsl-3d0@5000000`
- **Compatible String**: `qcom,kgsl-3d0, qcom,kgsl-3d`
- **MMIO Region**: Base `0x05000000` | Size `0x00090000` bytes
- **Power Supplies**: `vddcx-supply, vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,gpu-pwrlevel-bins`
- **Compatible String**: `qcom,gpu-pwrlevel-bins`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,kgsl-iommu@0x050a0000`
- **Compatible String**: `qcom,kgsl-smmu-v2`
- **MMIO Region**: Base `0x050A0000` | Size `0x00010000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `gfx3d_secure`
- **Compatible String**: `qcom,smmu-kgsl-cb`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,rgmu@0x0506d000`
- **Compatible String**: `qcom,gpu-rgmu`
- **MMIO Region**: Base `0x0506D000` | Size `0x00031000` bytes
- **Power Supplies**: `vddcx-supply, vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `hsusb@a800000`
- **Compatible String**: `qcom,dwc-usb3-msm`
- **MMIO Region**: Base `0x0A800000` | Size `0x00100000` bytes
- **Power Supplies**: `USB3_GDSC-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,usbbam@a704000`
- **Compatible String**: `qcom,usb-bam-msm`
- **MMIO Region**: Base `0x0A704000` | Size `0x00017000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qusb@88e3000`
- **Compatible String**: `qcom,qusb2phy`
- **MMIO Region**: Base `0x088E3000` | Size `0x00000180` bytes
- **Power Supplies**: `vdd-supply, vdda18-supply, vdda33-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `ssphy@88e6000`
- **Compatible String**: `qcom,usb-ssphy-qmp-usb3-or-dp`
- **MMIO Region**: Base `0x088E6000` | Size `0x00001000` bytes
- **Power Supplies**: `vdd-supply, core-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `usb_audio_qmi_dev`
- **Compatible String**: `qcom,usb-audio-qmi-dev`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `usb_nop_phy`
- **Compatible String**: `usb-nop-xceiv`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam-req-mgr`
- **Compatible String**: `qcom,cam-req-mgr`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,csiphy@ac67000`
- **Compatible String**: `qcom,csiphy-v2.0, qcom,csiphy`
- **MMIO Region**: Base `0x0AC67000` | Size `0x00001000` bytes
- **Power Supplies**: `gdscr-supply, mipi-csi-vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cci@ac4a000`
- **Compatible String**: `qcom,cci`
- **MMIO Region**: Base `0x0AC4A000` | Size `0x00004000` bytes
- **Power Supplies**: `gdscr-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam-res-mgr`
- **Compatible String**: `qcom,cam-res-mgr`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,actuator@1`
- **Compatible String**: `qcom,actuator`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `cam_vaf-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,ois@0`
- **Compatible String**: `qcom,ois`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `cam_vaf-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,eeprom@2`
- **Compatible String**: `qcom,eeprom`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `cam_vio-supply, cam_vana-supply, cam_vdig-supply, cam_clk-supply, cam_vaf-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam-sensor@2`
- **Compatible String**: `qcom,cam-sensor`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `cam_vio-supply, cam_vana-supply, cam_vdig-supply, cam_clk-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam_smmu`
- **Compatible String**: `qcom,msm-cam-smmu`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `msm_cam_smmu_secure`
- **Compatible String**: `qcom,msm-cam-smmu-cb`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `msm_cam_icp_fw`
- **Compatible String**: `qcom,msm-cam-smmu-fw-dev`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam-cpas@ac40000`
- **Compatible String**: `qcom,cam-cpas`
- **MMIO Region**: Base `0x0AC40000` | Size `0x00001000` bytes
- **Power Supplies**: `camss-vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam-cdm-intf`
- **Compatible String**: `qcom,cam-cdm-intf`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cpas-cdm0@ac48000`
- **Compatible String**: `qcom,cam170-cpas-cdm0`
- **MMIO Region**: Base `0x0AC48000` | Size `0x00001000` bytes
- **Power Supplies**: `camss-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam-isp`
- **Compatible String**: `qcom,cam-isp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,csid1@acba000`
- **Compatible String**: `qcom,csid170`
- **MMIO Region**: Base `0x0ACBA000` | Size `0x00001000` bytes
- **Power Supplies**: `camss-supply, ife1-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,vfe1@acb6000`
- **Compatible String**: `qcom,vfe170`
- **MMIO Region**: Base `0x0ACB6000` | Size `0x00004000` bytes
- **Power Supplies**: `camss-supply, ife1-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,csid-lite@acc8000`
- **Compatible String**: `qcom,csid-lite170`
- **MMIO Region**: Base `0x0ACC8000` | Size `0x00001000` bytes
- **Power Supplies**: `camss-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,vfe-lite@acc4000`
- **Compatible String**: `qcom,vfe-lite170`
- **MMIO Region**: Base `0x0ACC4000` | Size `0x00004000` bytes
- **Power Supplies**: `camss-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam-icp`
- **Compatible String**: `qcom,cam-icp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,a5@ac00000`
- **Compatible String**: `qcom,cam-a5`
- **MMIO Region**: Base `0x0AC00000` | Size `0x00006000` bytes
- **Power Supplies**: `camss-vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,ipe0`
- **Compatible String**: `qcom,cam-ipe`
- **MMIO Region**: Base `0x0AC87000` | Size `0x00003000` bytes
- **Power Supplies**: `ipe0-vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,bps`
- **Compatible String**: `qcom,cam-bps`
- **MMIO Region**: Base `0x0AC6F000` | Size `0x00003000` bytes
- **Power Supplies**: `bps-vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam-jpeg`
- **Compatible String**: `qcom,cam-jpeg`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,jpegenc@ac4e000`
- **Compatible String**: `qcom,cam_jpeg_enc`
- **MMIO Region**: Base `0x0AC4E000` | Size `0x00004000` bytes
- **Power Supplies**: `camss-vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,jpegdma@ac52000`
- **Compatible String**: `qcom,cam_jpeg_dma`
- **MMIO Region**: Base `0x0AC52000` | Size `0x00004000` bytes
- **Power Supplies**: `camss-vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,cam-lrme`
- **Compatible String**: `qcom,cam-lrme`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,lrme@ac6b000`
- **Compatible String**: `qcom,lrme`
- **MMIO Region**: Base `0x0AC6B000` | Size `0x00000A00` bytes
- **Power Supplies**: `camss-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,ion`
- **Compatible String**: `qcom,msm-ion`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `apps-smmu@0x15000000`
- **Compatible String**: `qcom,qsmmu-v500`
- **MMIO Region**: Base `0x15000000` | Size `0x00080000` bytes
- **Power Supplies**: `vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `adsp_tbu@0x150d9000`
- **Compatible String**: `qcom,qsmmuv500-tbu`
- **MMIO Region**: Base `0x150D9000` | Size `0x00001000` bytes
- **Power Supplies**: `vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `apps_iommu_coherent_test_device`
- **Compatible String**: `iommu-debug-test`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `csr@6b0e000`
- **Compatible String**: `qcom,coresight-csr`
- **MMIO Region**: Base `0x06B0E000` | Size `0x00001000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `dummy_sink`
- **Compatible String**: `qcom,coresight-dummy`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `turing_etm0`
- **Compatible String**: `qcom,coresight-remote-etm`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `hwevent@91866f0`
- **Compatible String**: `qcom,coresight-hwevent`
- **MMIO Region**: Base `0x091866F0` | Size `0x00000004` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `ad-hoc-bus`
- **Compatible String**: `qcom,msm-bus-device`
- **MMIO Region**: Base `0x01700000` | Size `0x00040000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,vidc@aa00000`
- **Compatible String**: `qcom,msm-vidc, qcom,sm6150-vidc`
- **MMIO Region**: Base `0x0AA00000` | Size `0x00200000` bytes
- **Power Supplies**: `venus-supply, venus-core0-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `arm9_bus_ddr`
- **Compatible String**: `qcom,msm-vidc,bus`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `secure_non_pixel_cb`
- **Compatible String**: `qcom,msm-vidc,context-bank`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-ultra-low-latency`
- **Compatible String**: `qcom,msm-pcm-dsp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-pcm-routing`
- **Compatible String**: `qcom,msm-pcm-routing`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-compr-dsp`
- **Compatible String**: `qcom,msm-compr-dsp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-pcm-dsp-noirq`
- **Compatible String**: `qcom,msm-pcm-dsp-noirq`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-transcode-loopback`
- **Compatible String**: `qcom,msm-transcode-loopback`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-compress-dsp`
- **Compatible String**: `qcom,msm-compress-dsp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-voip-dsp`
- **Compatible String**: `qcom,msm-voip-dsp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-pcm-voice`
- **Compatible String**: `qcom,msm-pcm-voice`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-stub-codec`
- **Compatible String**: `qcom,msm-stub-codec`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-fe`
- **Compatible String**: `qcom,msm-dai-fe`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-pcm-afe`
- **Compatible String**: `qcom,msm-pcm-afe`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-q6-dp`
- **Compatible String**: `qcom,msm-dai-q6-hdmi`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-pcm-loopback-low-latency`
- **Compatible String**: `qcom,msm-pcm-loopback`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-pcm-dtmf`
- **Compatible String**: `qcom,msm-pcm-dtmf`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-mi2s`
- **Compatible String**: `qcom,msm-dai-mi2s`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-q6-mi2s-senary`
- **Compatible String**: `qcom,msm-dai-q6-mi2s`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-q6-meta-mi2s-sec`
- **Compatible String**: `qcom,msm-dai-q6-meta-mi2s`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-cdc-dma`
- **Compatible String**: `qcom,msm-dai-cdc-dma`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-tx-cdc-dma-5-tx`
- **Compatible String**: `qcom,msm-dai-cdc-dma-dev`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-lsm-client`
- **Compatible String**: `qcom,msm-lsm-client`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-q6`
- **Compatible String**: `qcom,msm-dai-q6`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-q6-afe-loopback-tx`
- **Compatible String**: `qcom,msm-dai-q6-dev`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-pcm-hostless`
- **Compatible String**: `qcom,msm-pcm-hostless`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-audio-apr`
- **Compatible String**: `qcom,msm-audio-apr`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,q6core-audio`
- **Compatible String**: `qcom,q6core-audio`
- **MMIO Region**: Base `0x62F20000` | Size `0x00000000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `tx-macro@62ec0000`
- **Compatible String**: `qcom,tx-macro`
- **MMIO Region**: Base `0x62EC0000` | Size `0x00000000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `rx-macro@62ee0000`
- **Compatible String**: `qcom,rx-macro`
- **MMIO Region**: Base `0x62EE0000` | Size `0x00000000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `wsa-macro@62f00000`
- **Compatible String**: `qcom,wsa-macro`
- **MMIO Region**: Base `0x62F00000` | Size `0x00000000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `wsa881x@21170214`
- **Compatible String**: `qcom,wsa881x`
- **MMIO Region**: Base `0x00000000` | Size `0x21170214` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `wcd937x-codec`
- **Compatible String**: `qcom,wcd937x-codec`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `cdc-vdd-ldo-rxtx-supply, cdc-vddpx-1-supply, cdc-vdd-buck-supply, cdc-vdd-mic-bias-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `sound`
- **Compatible String**: `qcom,sm6150-asoc-snd`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-sen-auxpcm`
- **Compatible String**: `qcom,msm-auxpcm-dev`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-hdmi-dba-codec-rx`
- **Compatible String**: `qcom,msm-hdmi-dba-codec-rx`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-adsp-loader`
- **Compatible String**: `qcom,adsp-loader`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-tdm-sen-tx`
- **Compatible String**: `qcom,msm-dai-tdm`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-dai-q6-spdif-sec-tx`
- **Compatible String**: `qcom,msm-dai-q6-spdif`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,avtimer@62CF7000`
- **Compatible String**: `qcom,avtimer`
- **MMIO Region**: Base `0x62CF700C` | Size `0x00000004` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `audio_ext_clk`
- **Compatible String**: `qcom,audio-ref-clk`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,mdss_dsi_pll@ae94400`
- **Compatible String**: `qcom,mdss_dsi_pll_14nm`
- **MMIO Region**: Base `0x0AE94400` | Size `0x00000588` bytes
- **Power Supplies**: `gdsc-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,mdss_dp_pll@88e9000`
- **Compatible String**: `qcom,mdss_dp_pll_14nm`
- **MMIO Region**: Base `0x088E9C00` | Size `0x00000200` bytes
- **Power Supplies**: `gdsc-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,mdss_mdp@ae00000`
- **Compatible String**: `qcom,sde-kms`
- **MMIO Region**: Base `0x0AE00000` | Size `0x00084208` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smmu_sde_sec_cb`
- **Compatible String**: `qcom,smmu_sde_sec`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,sde_rscc@af20000`
- **Compatible String**: `qcom,sde-rsc`
- **MMIO Region**: Base `0x0AF20000` | Size `0x00001C44` bytes
- **Power Supplies**: `vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,mdss_rotator@ae00000`
- **Compatible String**: `qcom,sde_rotator`
- **MMIO Region**: Base `0x0AE00000` | Size `0x000AC000` bytes
- **Power Supplies**: `rot-vdd-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smmu_rot_unsec_cb`
- **Compatible String**: `qcom,smmu_sde_rot_unsec`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smmu_rot_sec_cb`
- **Compatible String**: `qcom,smmu_sde_rot_sec`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,mdss_dsi_ctrl0@ae94000`
- **Compatible String**: `qcom,dsi-ctrl-hw-v2.3`
- **MMIO Region**: Base `0x0AE94000` | Size `0x00000400` bytes
- **Power Supplies**: `vdda-1p2-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,mdss_dsi_phy0@ae94400`
- **Compatible String**: `qcom,dsi-phy-v2.0`
- **MMIO Region**: Base `0x0AE94400` | Size `0x00000588` bytes
- **Power Supplies**: `vdda-0p9-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-ext-disp`
- **Compatible String**: `qcom,msm-ext-disp`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,dp_display@0`
- **Compatible String**: `qcom,dp-display`
- **MMIO Region**: Base `0x0AE90000` | Size `0x000000F4` bytes
- **Power Supplies**: `vdda-1p2-supply, vdda-0p9-supply, hpd-pwr-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smp2p_interrupt_rdbg_2_out`
- **Compatible String**: `qcom,smp2p-interrupt-rdbg-2-out`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smp2p_interrupt_rdbg_2_in`
- **Compatible String**: `qcom,smp2p-interrupt-rdbg-2-in`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smp2p_interrupt_rdbg_5_out`
- **Compatible String**: `qcom,smp2p-interrupt-rdbg-5-out`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,smp2p_interrupt_rdbg_5_in`
- **Compatible String**: `qcom,smp2p-interrupt-rdbg-5-in`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,camera-flash@0`
- **Compatible String**: `qcom,camera-flash`
- **MMIO Region**: Base `0x00000000` | Size `0x00000000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `dbu1`
- **Compatible String**: `regulator-fixed`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,dsi-display`
- **Compatible String**: `qcom,dsi-display`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `vddio-supply, vdda-3p3-supply, lab-supply, ibb-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,wb-display@0`
- **Compatible String**: `qcom,wb-display`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `gpio_keys`
- **Compatible String**: `gpio-keys`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,emac@20000`
- **Compatible String**: `qcom,emac-dwc-eqos`
- **MMIO Region**: Base `0x00020000` | Size `0x00010000` bytes
- **Power Supplies**: `gdsc_emac-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `emac_emb_smmu`
- **Compatible String**: `qcom,emac-smmu-embedded`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `lpi_pinctrl@62b40000`
- **Compatible String**: `qcom,lpi-pinctrl`
- **MMIO Region**: Base `0x62B40000` | Size `0x00000000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `msm_cdc_pinctrl_pri`
- **Compatible String**: `qcom,msm-cdc-pinctrl`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `wcd9xxx-irq`
- **Compatible String**: `qcom,wcd9xxx-irq`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,wcd-dsp-glink`
- **Compatible String**: `qcom,wcd-dsp-glink`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `wcd-dsp-mgr@2`
- **Compatible String**: `qcom,wcd-dsp-mgr`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `goodix_fp`
- **Compatible String**: `goodix,fingerprint`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `vcc_spi-supply, vcc_ana-supply, vdd_io-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `fpc1020`
- **Compatible String**: `fpc,fpc1020`
- **MMIO Region**: NOT SPECIFIED
- **Power Supplies**: `vcc_spi-supply, vdd_io-supply, vdd_ana-supply`
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `qcom,msm-thermal-simple`
- **Compatible String**: `qcom,msm-thermal-simple`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `energy-costs`
- **Compatible String**: `sched-energy`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `psci`
- **Compatible String**: `arm,psci-1.0`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `firmware`
- **Compatible String**: `android,firmware`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `fstab`
- **Compatible String**: `android,fstab`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `persist`
- **Compatible String**: `android,persist`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `cdsp_sec_regions@0x9f800000`
- **Compatible String**: `removed-dma-pool`
- **MMIO Region**: Base `0x00000000` | Size `0x9F800000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `linux,cma`
- **Compatible String**: `shared-dma-pool`
- **MMIO Region**: NOT SPECIFIED
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

### Device Node: `ramoops@a1600000`
- **Compatible String**: `ramoops`
- **MMIO Region**: Base `0x00000000` | Size `0xA1600000` bytes
- **QEMU virt Status**: `NOT PRESENT IN virt.dtb`

## Dependency Graph
```text
cpu@700

```
```text
l1-dcache

```
```text
soc

```
```text
interrupt-controller@b220000

```
```text
qcom,memshare

```
```text
qcom,client_3

```
```text
timer@0x17c20000

```
```text
scc_pll_out_aux2

```
```text
scc_pll_out_aux

```
```text
qcom,rpmhclk

```
```text
qcom,aopclk

```
```text
qcom,gcc@100000
    +-- vdd_cx-supply
    +-- vdd_cx_ao-supply
```
```text
syscon@5091540

```
```text
qcom,videocc@ab00000
    +-- vdd_cx-supply
```
```text
qcom,camcc@ad00000
    +-- vdd_cx-supply
    +-- vdd_mx-supply
```
```text
qcom,dispcc@af00000
    +-- vdd_cx-supply
```
```text
qcom,gpupcc@5090000
    +-- vdd_cx-supply
    +-- vdd_mx-supply
```
```text
qcom,scc@62b10000
    +-- vdd_scc_cx-supply
```
```text
qcom,cpucc@18321000

```
```text
qcom,limits-dcvs@18350800

```
```text
qcom,cc-debug

```
```text
cpu-pmu

```
```text
dsu_pmu@0

```
```text
qcom,msm-imem@146aa000

```
```text
restart_reason@65c

```
```text
dload_type@1c

```
```text
boot_stats@6b0

```
```text
kaslr_offset@6d0

```
```text
pil@94c

```
```text
diag_dload@c8

```
```text
restart@c264000

```
```text
qcom,mpm2-sleep-counter@0xc221000

```
```text
qcom,sps

```
```text
qcom,gpi-dma@0xa00000

```
```text
aop-msg-client

```
```text
qcom,msm-rtb

```
```text
qcom,wdt@17c10000

```
```text
qcom,chd_gold

```
```text
kryo-erp

```
```text
qcom,ghd

```
```text
cpuss_dump

```
```text
mem_dump

```
```text
mailbox@af20000

```
```text
system_pm

```
```text
qcom,cmd-db@c3f000c

```
```text
dcc_v2@10a2000

```
```text
qcom,llcc@9200000

```
```text
qcom,llcc-perfmon

```
```text
qcom,llcc-erp

```
```text
qcom,llcc-amon

```
```text
ufsice@1d90000
    +-- vdd-hba-supply
```
```text
sdhci@8804000
    +-- vdd-supply
    +-- vdd-io-supply
```
```text
qseecom@86d00000

```
```text
smcinvoke@86d00000

```
```text
qrng@793000

```
```text
ufsphy_mem@1d87000
    +-- vdda-phy-supply
    +-- vdda-pll-supply
```
```text
ufshc@1d84000
    +-- vdd-hba-supply
    +-- vcc-supply
    +-- vccq2-supply
    +-- vddp-ref-clk-supply
```
```text
qcedev@1de0000

```
```text
qcom,msm_hdcp

```
```text
qcrypto@1de0000

```
```text
tz-log@146aa720

```
```text
qcom,spmi@c440000

```
```text
qcom,power-on@800

```
```text
vadc@3100

```
```text
adc_tm@3500

```
```text
qcom,misc@900

```
```text
qcom,qpnp-smb5
    +-- dpdm-supply
```
```text
qcom,usb-pdphy@1700
    +-- vdd-pdphy-supply
    +-- vbus-supply
    +-- vconn-supply
```
```text
qpnp,qg

```
```text
bcl@3d00

```
```text
bcl-soc

```
```text
qcom,temp-alarm@2400

```
```text
clock-controller@5b00

```
```text
pinctrl@c000

```
```text
qcom,pm6150_rtc

```
```text
qcom,pm6150l@5

```
```text
qcom,lcdb@ec00

```
```text
qcom,leds@d300

```
```text
qcom,wled@d800

```
```text
qcom,pwms@b100

```
```text
qcom,leds@d000

```
```text
qcom,amoled

```
```text
bt_wcn3990
    +-- bt-vdd-core-supply
    +-- bt-vdd-pa-supply
    +-- bt-vdd-ldo-supply
```
```text
qcom,msm-eud@88e0000

```
```text
slim@62e40000

```
```text
msm_dai_slim

```
```text
tavil_codec
    +-- cdc-vdd-buck-supply
    +-- cdc-buck-sido-supply
    +-- cdc-vdd-tx-h-supply
    +-- cdc-vdd-rx-h-supply
    +-- cdc-vddpx-1-supply
    +-- cdc-vdd-mic-bias-supply
    +-- vreg-micb-supply
```
```text
wcn3990

```
```text
hwlock

```
```text
qcom,smem

```
```text
mailbox@17c00000

```
```text
qcom,msm-cdsp-loader

```
```text
qcom,msm-adsprpc-mem

```
```text
qcom,msm_fastrpc

```
```text
qcom,msm_fastrpc_compute_cb13

```
```text
qcom,glink

```
```text
qcom,msm_fastrpc_rpmsg

```
```text
qcom,msm_cdsprm_rpmsg

```
```text
qcom,msm_cdsp_rm

```
```text
qcom,msm_hvx_rm

```
```text
qcom,glinkpkt

```
```text
qcom,qmp-aop@c300000

```
```text
qcom,smp2p_sleepstate

```
```text
qcom,smp2p-cdsp

```
```text
tsens@c222000

```
```text
qcom,kgsl-hyp

```
```text
qcom,msm_gsi

```
```text
qcom,rmnet-ipa

```
```text
qcom,ipa@1e00000

```
```text
qcom,smp2p_map_ipa_1_in

```
```text
ipa_smmu_ap

```
```text
ipa_smmu_wlan

```
```text
ipa_smmu_uc

```
```text
qcom,icnss@18800000
    +-- vdd-cx-mx-supply
    +-- 8-xo-supply
    +-- 3-rfa-supply
    +-- 3-ch0-supply
```
```text
qcom,rmtfs_sharedmem@0

```
```text
llcc-pmu@90cc000

```
```text
keepalive-opp-table

```
```text
qcom,gpubw

```
```text
qcom,cpu-cpu-llcc-bwmon@90b6400

```
```text
qcom,cpu-llcc-ddr-bwmon@90cd000

```
```text
qcom,cpu6-cpu-l3-lat

```
```text
qcom,cpu6-llcc-ddr-latmon

```
```text
qcom,cpu6-computemon

```
```text
qcom,bus_proxy_client

```
```text
cx_ipeak@01fed000

```
```text
demux

```
```text
pinctrl@03000000

```
```text
slpi_pinctrl@62B40000

```
```text
rpmh-regulator-lcxlvl

```
```text
mx-cdev-lvl
    +-- regulator-cdev-supply
```
```text
regulator-cdev

```
```text
rpmh-regulator-bobc1

```
```text
qcom,lpm-levels

```
```text
qcom,rpm-stats@c300000

```
```text
qcom,rpmh-master-stats@b221200

```
```text
qcom,gdsc@ab00814

```
```text
qcom,qupv3_2_geni_se@626c0000

```
```text
qcom,qup_uart@0x880000

```
```text
i2c@62694000

```
```text
novatek@62

```
```text
novatek-mp-criteria-5916@0

```
```text
focaltech@38

```
```text
qcom,smb1355@c

```
```text
qcom,charge_pump

```
```text
qcom,smb1355-charger@1000

```
```text
spi@62688000

```
```text
qcom,qup_uart@0xa8c000

```
```text
qmi-tmd-devices

```
```text
cxip-cdev@1fed000

```
```text
qcom,kgsl-busmon

```
```text
qcom,kgsl-3d0@5000000
    +-- vddcx-supply
    +-- vdd-supply
```
```text
qcom,gpu-pwrlevel-bins

```
```text
qcom,kgsl-iommu@0x050a0000

```
```text
gfx3d_secure

```
```text
qcom,rgmu@0x0506d000
    +-- vddcx-supply
    +-- vdd-supply
```
```text
hsusb@a800000
    +-- USB3_GDSC-supply
```
```text
qcom,usbbam@a704000

```
```text
qusb@88e3000
    +-- vdd-supply
    +-- vdda18-supply
    +-- vdda33-supply
```
```text
ssphy@88e6000
    +-- vdd-supply
    +-- core-supply
```
```text
usb_audio_qmi_dev

```
```text
usb_nop_phy

```
```text
qcom,cam-req-mgr

```
```text
qcom,csiphy@ac67000
    +-- gdscr-supply
    +-- mipi-csi-vdd-supply
```
```text
qcom,cci@ac4a000
    +-- gdscr-supply
```
```text
qcom,cam-res-mgr

```
```text
qcom,actuator@1
    +-- cam_vaf-supply
```
```text
qcom,ois@0
    +-- cam_vaf-supply
```
```text
qcom,eeprom@2
    +-- cam_vio-supply
    +-- cam_vana-supply
    +-- cam_vdig-supply
    +-- cam_clk-supply
    +-- cam_vaf-supply
```
```text
qcom,cam-sensor@2
    +-- cam_vio-supply
    +-- cam_vana-supply
    +-- cam_vdig-supply
    +-- cam_clk-supply
```
```text
qcom,cam_smmu

```
```text
msm_cam_smmu_secure

```
```text
msm_cam_icp_fw

```
```text
qcom,cam-cpas@ac40000
    +-- camss-vdd-supply
```
```text
qcom,cam-cdm-intf

```
```text
qcom,cpas-cdm0@ac48000
    +-- camss-supply
```
```text
qcom,cam-isp

```
```text
qcom,csid1@acba000
    +-- camss-supply
    +-- ife1-supply
```
```text
qcom,vfe1@acb6000
    +-- camss-supply
    +-- ife1-supply
```
```text
qcom,csid-lite@acc8000
    +-- camss-supply
```
```text
qcom,vfe-lite@acc4000
    +-- camss-supply
```
```text
qcom,cam-icp

```
```text
qcom,a5@ac00000
    +-- camss-vdd-supply
```
```text
qcom,ipe0
    +-- ipe0-vdd-supply
```
```text
qcom,bps
    +-- bps-vdd-supply
```
```text
qcom,cam-jpeg

```
```text
qcom,jpegenc@ac4e000
    +-- camss-vdd-supply
```
```text
qcom,jpegdma@ac52000
    +-- camss-vdd-supply
```
```text
qcom,cam-lrme

```
```text
qcom,lrme@ac6b000
    +-- camss-supply
```
```text
qcom,ion

```
```text
apps-smmu@0x15000000
    +-- vdd-supply
```
```text
adsp_tbu@0x150d9000
    +-- vdd-supply
```
```text
apps_iommu_coherent_test_device

```
```text
csr@6b0e000

```
```text
dummy_sink

```
```text
turing_etm0

```
```text
hwevent@91866f0

```
```text
ad-hoc-bus

```
```text
qcom,vidc@aa00000
    +-- venus-supply
    +-- venus-core0-supply
```
```text
arm9_bus_ddr

```
```text
secure_non_pixel_cb

```
```text
qcom,msm-ultra-low-latency

```
```text
qcom,msm-pcm-routing

```
```text
qcom,msm-compr-dsp

```
```text
qcom,msm-pcm-dsp-noirq

```
```text
qcom,msm-transcode-loopback

```
```text
qcom,msm-compress-dsp

```
```text
qcom,msm-voip-dsp

```
```text
qcom,msm-pcm-voice

```
```text
qcom,msm-stub-codec

```
```text
qcom,msm-dai-fe

```
```text
qcom,msm-pcm-afe

```
```text
qcom,msm-dai-q6-dp

```
```text
qcom,msm-pcm-loopback-low-latency

```
```text
qcom,msm-pcm-dtmf

```
```text
qcom,msm-dai-mi2s

```
```text
qcom,msm-dai-q6-mi2s-senary

```
```text
qcom,msm-dai-q6-meta-mi2s-sec

```
```text
qcom,msm-dai-cdc-dma

```
```text
qcom,msm-dai-tx-cdc-dma-5-tx

```
```text
qcom,msm-lsm-client

```
```text
qcom,msm-dai-q6

```
```text
qcom,msm-dai-q6-afe-loopback-tx

```
```text
qcom,msm-pcm-hostless

```
```text
qcom,msm-audio-apr

```
```text
qcom,q6core-audio

```
```text
tx-macro@62ec0000

```
```text
rx-macro@62ee0000

```
```text
wsa-macro@62f00000

```
```text
wsa881x@21170214

```
```text
wcd937x-codec
    +-- cdc-vdd-ldo-rxtx-supply
    +-- cdc-vddpx-1-supply
    +-- cdc-vdd-buck-supply
    +-- cdc-vdd-mic-bias-supply
```
```text
sound

```
```text
qcom,msm-sen-auxpcm

```
```text
qcom,msm-hdmi-dba-codec-rx

```
```text
qcom,msm-adsp-loader

```
```text
qcom,msm-dai-tdm-sen-tx

```
```text
qcom,msm-dai-q6-spdif-sec-tx

```
```text
qcom,avtimer@62CF7000

```
```text
audio_ext_clk

```
```text
qcom,mdss_dsi_pll@ae94400
    +-- gdsc-supply
```
```text
qcom,mdss_dp_pll@88e9000
    +-- gdsc-supply
```
```text
qcom,mdss_mdp@ae00000

```
```text
qcom,smmu_sde_sec_cb

```
```text
qcom,sde_rscc@af20000
    +-- vdd-supply
```
```text
qcom,mdss_rotator@ae00000
    +-- rot-vdd-supply
```
```text
qcom,smmu_rot_unsec_cb

```
```text
qcom,smmu_rot_sec_cb

```
```text
qcom,mdss_dsi_ctrl0@ae94000
    +-- vdda-1p2-supply
```
```text
qcom,mdss_dsi_phy0@ae94400
    +-- vdda-0p9-supply
```
```text
qcom,msm-ext-disp

```
```text
qcom,dp_display@0
    +-- vdda-1p2-supply
    +-- vdda-0p9-supply
    +-- hpd-pwr-supply
```
```text
qcom,smp2p_interrupt_rdbg_2_out

```
```text
qcom,smp2p_interrupt_rdbg_2_in

```
```text
qcom,smp2p_interrupt_rdbg_5_out

```
```text
qcom,smp2p_interrupt_rdbg_5_in

```
```text
qcom,camera-flash@0

```
```text
dbu1

```
```text
qcom,dsi-display
    +-- vddio-supply
    +-- vdda-3p3-supply
    +-- lab-supply
    +-- ibb-supply
```
```text
qcom,wb-display@0

```
```text
gpio_keys

```
```text
qcom,emac@20000
    +-- gdsc_emac-supply
```
```text
emac_emb_smmu

```
```text
lpi_pinctrl@62b40000

```
```text
msm_cdc_pinctrl_pri

```
```text
wcd9xxx-irq

```
```text
qcom,wcd-dsp-glink

```
```text
wcd-dsp-mgr@2

```
```text
goodix_fp
    +-- vcc_spi-supply
    +-- vcc_ana-supply
    +-- vdd_io-supply
```
```text
fpc1020
    +-- vcc_spi-supply
    +-- vdd_io-supply
    +-- vdd_ana-supply
```
```text
qcom,msm-thermal-simple

```
```text
energy-costs

```
```text
psci

```
```text
firmware

```
```text
fstab

```
```text
persist

```
```text
cdsp_sec_regions@0x9f800000

```
```text
linux,cma

```
```text
ramoops@a1600000

```
```
