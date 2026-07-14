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

    // Redirect serial logs to stdio, and tell guest kernel to output to the virt serial controller
    if (!params.cmdline.empty()) {
        args << "-append" << QString::fromStdString(params.cmdline);
    } else {
        args << "-append" << "console=ttyAMA0 earlycon=pl011,0x09000000 printk.devkmsg=on";
    }

    args << "-serial" << "mon:stdio";

    if (!params.extraArgs.empty()) {
        QString extra = QString::fromStdString(params.extraArgs);
        QStringList extraParts = extra.split(' ', Qt::SkipEmptyParts);
        args.append(extraParts);
    }

    return args;
}
