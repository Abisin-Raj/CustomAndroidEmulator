target remote :1234
break *0xffffff800888a25c
continue

echo === Hit SMC breakpoint ===
info registers x0 x1 x2 x3 x4 x5 x6 x7 x8 sp pc lr

# Emulate: X0=0, X1=1
set $x0 = 0
set $x1 = 1
set $x2 = 0
set $x3 = 0

echo === Registers after emulation ===
info registers x0 x1 x2 x3

echo === Disassembly at PC ===
x/10i $pc

# Advance PC past smc
set $pc = $pc + 4
echo === Advanced PC past SMC ===
x/10i $pc

# Step through and log
echo === Step 1 ===
stepi
info registers x0 x1 x2 x3 x4 x5 x8 sp pc lr

echo === Step 2 ===
stepi
info registers x0 x1 x2 x3 x4 x5 x8 sp pc lr

echo === Step 3 ===
stepi
info registers x0 x1 x2 x3 x4 x5 x8 sp pc lr

echo === Step 4 ===
stepi
info registers x0 x1 x2 x3 x4 x5 x8 sp pc lr

echo === Step 5 ===
stepi
info registers x0 x1 x2 x3 x4 x5 x8 sp pc lr

echo === Step 6 ===
stepi
info registers x0 x1 x2 x3 x4 x5 x8 sp pc lr

echo === Step 7 ===
stepi
info registers x0 x1 x2 x3 x4 x5 x8 sp pc lr

quit
