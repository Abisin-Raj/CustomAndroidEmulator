# Analysis Report: Phase 8.6 Hardware Contract & Minimum Device Specification
**Execution Status**: SUCCESS

## Executive Summary
Extracted Qualcomm clock-controller and timer DTS nodes from kernel_extracted.dts/dtbo_extracted.dts. Disassembled entry [3] probe register contract. Determined that driver probe requires Device Tree node injection with Qualcomm clock compatible string ('qcom,gcc-sm6150' / 'qcom,gcc-violet') and a minimal MMIO register stub.

## Environment details
- **Target Structure X21**: 0xffffff800a931000
- **Disassembly PC**: 0xffffff80080dc230
- **Recommended Contract**: Option B: Device Tree Node Injection + Static MMIO Register Stub

## Register Dump
```text
=== CPU 0 Register Snapshot ===
  X0 =0x0000000000000001  X1 =0x0000000000000000  X2 =0x000000000000000c  X3 =0x000000002e2e7364
  X4 =0x000000000000000c  X5 =0xffffff800a93579c  X6 =0xffffff800a957bcc  X7 =0x0000000000000000
  X8 =0xffffff8008080238  X9 =0x0000000000000002  X10=0x0000000000000000  X11=0xffffffffffffffff
  X12=0xffffff800a957bb2  X13=0x0000000000000000  X14=0x0000000000000001  X15=0xffffff800a957000
  X16=0x00000000000000e4  X17=0x0000000000000000  X18=0xffffff800a604048  X19=0xffffff800a8a3180
  X20=0x0000000000000001  X21=0xffffff800a931000  X22=0xffffff8008080230  X23=0x0000000000000000
  X24=0xffffff800a6f1000  X25=0x00000000000003e8  X26=0x0000000000000000  X27=0x000000004cccf320
  X28=0xffffff800a8a2000  X29=0xffffff800a6039c0  X30=0xffffff80080dc1d8
  SP =0xffffff800a6038b0   PC =0xffffff80080dc230   LR =0xffffff80080dc1d8
  PSTATE=0x00000000  CurrentEL=0
--- System EL1 Control Registers ---
  SCTLR =0x0000000000000000   TTBR0=0x0000000000000000
  TTBR1 =0x0000000000000000   TCR  =0x0000000000000000
  MAIR  =0x0000000000000000   ESR  =0x0000000000000000   FAR=0x0000000000000000
```

## Disassembly Context
```text
--- Extracted Qualcomm DTS Nodes ---
### kernel_extracted.dts
```dts
timer {
compatible = "arm,armv8-timer";
interrupts = <0x01 0x01 0xf08 0x01 0x02 0xf08 0x01 0x03 0xf08 0x01 0x00 0xf08>;
clock-frequency = <0x124f800>;
};
```
```dts
timer@0x17c20000 {
#address-cells = <0x01>;
#size-cells = <0x01>;
ranges;
compatible = "arm,armv7-timer-mem";
reg = <0x17c20000 0x1000>;
clock-frequency = <0x124f800>;

frame@0x17c21000 {
frame-number = <0x00>;
```
```dts
qcom,gcc@100000 {
compatible = "qcom,gcc-sm6150", "syscon";
reg = <0x100000 0x1f0000>;
reg-names = "cc_base";
vdd_cx-supply = <0x1d>;
vdd_cx_ao-supply = <0x1e>;
protected-clocks = <0xb0 0xb1 0xb2 0xb3 0xb4 0xb5 0xb6>;
#clock-cells = <0x01>;
#reset-cells = <0x01>;
phandle = <0x31>;
```
### dtbo_extracted.dts
```dts
qcom,s1-timer = <0x1a40>;
qcom,s2-timer = <0x7d0>;
qcom,s2-type = <0x07>;
};
```
```dts
qcom,s1-timer = <0xc00>;
qcom,s2-timer = <0x7d0>;
qcom,s2-type = <0x01>;
qcom,use-back;
};
```
```dts
clock_gcc = "/fragment@20/__overlay__/qcom,emac@20000:clocks:0", "/fragment@20/__overlay__/qcom,emac@20000:clocks:8", "/fragment@20/__overlay__/qcom,emac@20000:clocks:16", "/fragment@20/__overlay__/qcom,emac@20000:clocks:24";
emac_gdsc = "/fragment@20/__overlay__/qcom,emac@20000:gdsc_emac-supply:0";
emac_mdc = "/fragment@20/__overlay__/qcom,emac@20000:pinctrl-0:0";
emac_mdio = "/fragment@20/__overlay__/qcom,emac@20000:pinctrl-1:0";
emac_rgmii_txd0 = "/fragment@20/__overlay__/qcom,emac@20000:pinctrl-2:0";
emac_rgmii_txd1 = "/fragment@20/__overlay__/qcom,emac@20000:pinctrl-3:0";
emac_rgmii_txd2 = "/fragment@20/__overlay__/qcom,emac@20000:pinctrl-4:0";
emac_rgmii_txd3 = "/fragment@20/__overlay__/qcom,emac@20000:pinctrl-5:0";
emac_rgmii_txc = "/fragment@20/__overlay__/qcom,emac@20000:pinctrl-6:0";
emac_rgmii_tx_ctl = "/fragment@20/__overlay__/qcom,emac@20000:pinctrl-7:0";
```
--- Entry [3] Probe MMIO Accesses ---
0xffffff80080dd330: sub        sp, sp, #0xa0
0xffffff80080dd334: str        x30, [x18], #8
0xffffff80080dd338: stp        x29, x30, [sp, #0x40]
0xffffff80080dd33c: add        x29, sp, #0x40
0xffffff80080dd340: adrp       x8, #0xffffff800a654000
0xffffff80080dd344: ldr        x8, [x8, #0xe80]
0xffffff80080dd348: str        x27, [sp, #0x50]
0xffffff80080dd34c: stp        x26, x25, [sp, #0x60]
0xffffff80080dd350: stp        x24, x23, [sp, #0x70]
0xffffff80080dd354: mov        x23, x0
0xffffff80080dd358: stp        x22, x21, [sp, #0x80]
0xffffff80080dd35c: mov        w21, w2
0xffffff80080dd360: stp        x20, x19, [sp, #0x90]
0xffffff80080dd364: mov        x19, x4
0xffffff80080dd368: stur       x8, [x29, #-8]
0xffffff80080dd36c: mov        w20, w3
```
