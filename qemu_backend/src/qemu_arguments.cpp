#include "qemu_backend/qemu_arguments.h"
#include <QString>

QStringList QemuArguments::build(const QemuConfigParameters& params) {
    QStringList args;

    args << "-M" << QString::fromStdString(params.machine);
    args << "-cpu" << QString::fromStdString(params.cpu);
    args << "-m" << QString::fromStdString(params.memory);

    if (params.nographic) {
        args << "-nographic";
    } else if (params.enableVnc) {
        if (!params.displayDevice.empty()) {
            args << "-device" << QString::fromStdString(params.displayDevice);
        }
        args << "-vnc" << QString::fromStdString(params.vncAddress);
    }

    if (!params.kernelPath.empty()) {
        args << "-kernel" << QString::fromStdString(params.kernelPath);
    }

    if (!params.ramdiskPath.empty()) {
        args << "-initrd" << QString::fromStdString(params.ramdiskPath);
    }

    if (!params.dtbPath.empty()) {
        args << "-dtb" << QString::fromStdString(params.dtbPath);
    }


    // Redirect serial logs to stdio, and tell guest kernel to output to the virt serial controller
    if (!params.cmdline.empty()) {
        args << "-append" << QString::fromStdString(params.cmdline);
    } else {
        args << "-append" << "console=ttyAMA0 earlycon=pl011,0x09000000 printk.devkmsg=on";
    }

    args << "-serial" << "stdio";

    // MMIO trace: log every guest access to an unmapped/unimplemented region.
    // NOTE: -d unimp only captures what QEMU considers "unimplemented". If the
    // target range is mapped as silent RAM, use GDB Z2 watchpoints as fallback.
    if (params.enableMmioTrace) {
        args << "-d" << "int,unimp,guest_errors" << "-D" << "qemu_trace.log";
    }


    // DTB dump: QEMU writes its generated DTB to disk and exits.
    // Decompile with: dtc -I dtb -O dts qemu_generated.dtb
    if (params.enableDtbDump) {
        args << "-dumpdtb" << "qemu_generated.dtb";
    }

    // Network: user-mode + host-to-guest port forwarding for input_bridge TCP server.
    // input_bridge inside the guest listens on :7777. We forward host port
    // params.inputBridgeHostPort → guest :7777 so the Qt host can send key events.
    if (params.enableInputBridge) {
        QString hostfwd = QString("user,id=net0,hostfwd=tcp::%1-:7777").arg(params.inputBridgeHostPort);
        args << "-netdev" << hostfwd << "-device" << "virtio-net-pci,netdev=net0";
    }

    if (!params.extraArgs.empty()) {
        QString extra = QString::fromStdString(params.extraArgs);
        QStringList extraParts = extra.split(' ', Qt::SkipEmptyParts);
        args.append(extraParts);
    }

    return args;
}
