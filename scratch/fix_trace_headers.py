import os
import glob
import shutil

build_dir = r"C:\qemu_source\build"
src_trace_dir = r"C:\qemu_source\trace"
src_sub_trace_dir = os.path.join(src_trace_dir, "trace")

build_trace_dir = os.path.join(build_dir, "trace")
build_sub_trace_dir = os.path.join(build_trace_dir, "trace")

for d in [src_trace_dir, src_sub_trace_dir, build_trace_dir, build_sub_trace_dir]:
    os.makedirs(d, exist_ok=True)

all_headers = []
for root, dirs, files in os.walk(build_dir):
    for f in files:
        if f.endswith(".h"):
            all_headers.append(os.path.join(root, f))

for h_path in all_headers:
    fname = os.path.basename(h_path)
    variations = {fname}
    if "C__qemu_source_build_" in fname:
        v1 = fname.replace("C__qemu_source_build_", "")
        variations.add(v1)
        v2 = v1.replace("qapi_qapi_", "qapi_")
        variations.add(v2)
        if not v2.startswith("trace-"):
            variations.add("trace-" + v2)

    for v in variations:
        for target_dir in [src_trace_dir, src_sub_trace_dir, build_trace_dir, build_sub_trace_dir]:
            try:
                shutil.copyfile(h_path, os.path.join(target_dir, v))
            except Exception:
                pass

print("Ubiquitous trace header propagation complete.")
