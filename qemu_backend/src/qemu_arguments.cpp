#include "qemu_backend/qemu_arguments.h"
#include <QString>

QStringList QemuArguments::build(const QemuConfigParameters& params) {
    QStringList args;

    args << "-M" << QString::fromStdString(params.machine);
    args << "-cpu" << QString::fromStdString(params.cpu);
    args << "-m" << QString::fromStdString(params.memory);

    if (params.nographic) {
        args << "-nographic";
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

    args << "-serial" << "file:qemu_uart.log";

    // MMIO trace: log every guest access to an unmapped/unimplemented region.
    // NOTE: -d unimp only captures what QEMU considers "unimplemented". If the
    // target range is mapped as silent RAM, use GDB Z2 watchpoints as fallback.
    if (params.enableMmioTrace) {
        args << "-d" << "unimp,guest_errors" << "-D" << "qemu_trace.log";
    }

    // DTB dump: QEMU writes its generated DTB to disk and exits.
    // Decompile with: dtc -I dtb -O dts qemu_generated.dtb
    if (params.enableDtbDump) {
        args << "-dumpdtb" << "qemu_generated.dtb";
    }

    if (!params.extraArgs.empty()) {
        QString extra = QString::fromStdString(params.extraArgs);
        QStringList extraParts = extra.split(' ', Qt::SkipEmptyParts);
        args.append(extraParts);
    }

    return args;
}
