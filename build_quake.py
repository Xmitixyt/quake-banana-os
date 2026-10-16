import os
import subprocess
import sys
import shutil

SDK_DIR = r"C:\BananaOs\Banana-OS\sdk"
LLVM_DIR = r"C:\Program Files\LLVM\bin"
CLANG = os.path.join(LLVM_DIR, "clang.exe")
LLD = os.path.join(LLVM_DIR, "ld.lld.exe")
WINQUAKE_DIR = r"C:\BananaOs\idquake\WinQuake"
QUAKE_DIR = r"C:\BananaOs\quake"
PAK_FILE = r"C:\BananaOs\pak0.pak"
PAK1_FILE = r"C:\BananaOs\pak1.pak"
BPKG_TOOL = os.path.join(SDK_DIR, "tools", "bpkg")

SOURCES = [
    # Banana OS SDK libc
    os.path.join(SDK_DIR, "lib", "crt0.c"),
    os.path.join(SDK_DIR, "lib", "banana.c"),
    os.path.join(SDK_DIR, "lib", "libc.c"),
    os.path.join(SDK_DIR, "lib", "stdio.c"),
    os.path.join(SDK_DIR, "lib", "math.c"),
    os.path.join(SDK_DIR, "lib", "posix.c"),
    os.path.join(SDK_DIR, "lib", "setjmp.c"),
    os.path.join(SDK_DIR, "lib", "mman.c"),
    os.path.join(SDK_DIR, "lib", "pthread.c"),
    os.path.join(SDK_DIR, "lib", "socket.c"),
    os.path.join(SDK_DIR, "lib", "signal.c"),
    os.path.join(SDK_DIR, "lib", "dl.c"),
    os.path.join(SDK_DIR, "lib", "cxxrt.c"),
    os.path.join(SDK_DIR, "lib", "unix.c"),
    os.path.join(SDK_DIR, "lib", "ufd.c"),
    # Banana OS Quake HAL
    os.path.join(QUAKE_DIR, "sys_banana.c"),
    os.path.join(QUAKE_DIR, "vid_banana.c"),
    os.path.join(QUAKE_DIR, "snd_banana.c"),
    # WinQuake Core Engine
    os.path.join(WINQUAKE_DIR, "chase.c"),
    os.path.join(WINQUAKE_DIR, "cl_demo.c"),
    os.path.join(WINQUAKE_DIR, "cl_input.c"),
    os.path.join(WINQUAKE_DIR, "cl_main.c"),
    os.path.join(WINQUAKE_DIR, "cl_parse.c"),
    os.path.join(WINQUAKE_DIR, "cl_tent.c"),
    os.path.join(WINQUAKE_DIR, "cmd.c"),
    os.path.join(WINQUAKE_DIR, "common.c"),
    os.path.join(WINQUAKE_DIR, "console.c"),
    os.path.join(WINQUAKE_DIR, "crc.c"),
    os.path.join(WINQUAKE_DIR, "cvar.c"),
    os.path.join(WINQUAKE_DIR, "draw.c"),
    os.path.join(WINQUAKE_DIR, "d_edge.c"),
    os.path.join(WINQUAKE_DIR, "d_fill.c"),
    os.path.join(WINQUAKE_DIR, "d_init.c"),
    os.path.join(WINQUAKE_DIR, "d_modech.c"),
    os.path.join(WINQUAKE_DIR, "d_part.c"),
    os.path.join(WINQUAKE_DIR, "d_polyse.c"),
    os.path.join(WINQUAKE_DIR, "d_scan.c"),
    os.path.join(WINQUAKE_DIR, "d_sky.c"),
    os.path.join(WINQUAKE_DIR, "d_sprite.c"),
    os.path.join(WINQUAKE_DIR, "d_surf.c"),
    os.path.join(WINQUAKE_DIR, "d_vars.c"),
    os.path.join(WINQUAKE_DIR, "d_zpoint.c"),
    os.path.join(WINQUAKE_DIR, "host.c"),
    os.path.join(WINQUAKE_DIR, "host_cmd.c"),
    os.path.join(WINQUAKE_DIR, "keys.c"),
    os.path.join(WINQUAKE_DIR, "menu.c"),
    os.path.join(WINQUAKE_DIR, "mathlib.c"),
    os.path.join(WINQUAKE_DIR, "model.c"),
    os.path.join(WINQUAKE_DIR, "nonintel.c"),
    os.path.join(WINQUAKE_DIR, "pr_cmds.c"),
    os.path.join(WINQUAKE_DIR, "pr_edict.c"),
    os.path.join(WINQUAKE_DIR, "pr_exec.c"),
    os.path.join(WINQUAKE_DIR, "r_aclip.c"),
    os.path.join(WINQUAKE_DIR, "r_alias.c"),
    os.path.join(WINQUAKE_DIR, "r_bsp.c"),
    os.path.join(WINQUAKE_DIR, "r_light.c"),
    os.path.join(WINQUAKE_DIR, "r_draw.c"),
    os.path.join(WINQUAKE_DIR, "r_efrag.c"),
    os.path.join(WINQUAKE_DIR, "r_edge.c"),
    os.path.join(WINQUAKE_DIR, "r_misc.c"),
    os.path.join(WINQUAKE_DIR, "r_main.c"),
    os.path.join(WINQUAKE_DIR, "r_sky.c"),
    os.path.join(WINQUAKE_DIR, "r_sprite.c"),
    os.path.join(WINQUAKE_DIR, "r_surf.c"),
    os.path.join(WINQUAKE_DIR, "r_part.c"),
    os.path.join(WINQUAKE_DIR, "r_vars.c"),
    os.path.join(WINQUAKE_DIR, "screen.c"),
    os.path.join(WINQUAKE_DIR, "sbar.c"),
    os.path.join(WINQUAKE_DIR, "sv_main.c"),
    os.path.join(WINQUAKE_DIR, "sv_phys.c"),
    os.path.join(WINQUAKE_DIR, "sv_move.c"),
    os.path.join(WINQUAKE_DIR, "sv_user.c"),
    os.path.join(WINQUAKE_DIR, "zone.c"),
    os.path.join(WINQUAKE_DIR, "view.c"),
    os.path.join(WINQUAKE_DIR, "wad.c"),
    os.path.join(WINQUAKE_DIR, "world.c"),
    os.path.join(WINQUAKE_DIR, "cd_null.c"),
    os.path.join(WINQUAKE_DIR, "snd_dma.c"),
    os.path.join(WINQUAKE_DIR, "snd_mix.c"),
    os.path.join(WINQUAKE_DIR, "snd_mem.c"),
    os.path.join(WINQUAKE_DIR, "net_none.c"),
    os.path.join(WINQUAKE_DIR, "net_loop.c"),
    os.path.join(WINQUAKE_DIR, "net_main.c"),
    os.path.join(WINQUAKE_DIR, "net_vcr.c")
]

def build_target(arch):
    print(f"\n==========================================")
    print(f"Building Quake for {arch}...")
    print(f"==========================================")
    
    target_triple = "x86_64-unknown-linux-gnu" if arch == "x86_64" else "i686-unknown-linux-gnu"
    build_dir = os.path.join(QUAKE_DIR, f"build_{arch}")
    os.makedirs(build_dir, exist_ok=True)
    
    cflags = [
        f"--target={target_triple}",
        "-ffreestanding", "-nostdinc", "-fno-builtin",
        "-fpie", "-fvisibility=hidden",
        "-fstack-clash-protection",
        "-O3", "-ffast-math", "-funroll-loops",
        "-fcommon",
        "-Did386=0",
        "-DNO_X86_ASM",
        "-Wno-deprecated-non-prototype", "-Wno-pointer-sign",
        "-Wno-incompatible-pointer-types", "-Wno-implicit-function-declaration",
        "-Wno-int-conversion", "-Wno-return-type",
        "-I" + os.path.join(SDK_DIR, "include"),
        "-I" + WINQUAKE_DIR,
        "-I" + QUAKE_DIR
    ]
    if arch == "x86_64":
        cflags.append("-mno-red-zone")
    else:
        cflags.append("-m32")
        
    src_list = list(SOURCES)
    if arch == "i686":
        div_c = os.path.join(SDK_DIR, "lib", "divdi3.c")
        if os.path.exists(div_c):
            src_list.append(div_c)

    objs = []
    for src in src_list:
        base = os.path.splitext(os.path.basename(src))[0]
        prefix = "sdk_" if "sdk" in src else ""
        obj = os.path.join(build_dir, f"{prefix}{base}.o")
        objs.append(obj)

        if os.path.exists(obj) and os.path.getmtime(obj) > os.path.getmtime(src):
            continue

        cmd = [CLANG] + cflags + ["-c", src, "-o", obj]
        res = subprocess.run(cmd, capture_output=True, text=True)
        if res.returncode != 0:
            print(f"FAILED compiling {src}")
            print(res.stderr)
            sys.exit(1)
            
    print(f"Successfully compiled {len(objs)} objects for {arch}.")
    
    out_elf = os.path.join(QUAKE_DIR, f"app-{arch}")
    ldflags = [
        "-flavor", "gnu",
        "-pie", "--no-dynamic-linker",
        "-z", "noexecstack", "-z", "norelro",
        "-e", "_banana_start",
        "-s",
        "-o", out_elf
    ] + objs
    
    res = subprocess.run([LLD] + ldflags, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"FAILED linking {out_elf}")
        print(res.stderr)
        sys.exit(1)
        
    print(f"SUCCESS: {out_elf} created ({os.path.getsize(out_elf):,} bytes).")
    return out_elf

def main():
    elf_x86_64 = build_target("x86_64")
    elf_i686 = build_target("i686")
    
    print("\n==========================================")
    print("Packaging Quake Full Version (quake.bpk)...")
    print("==========================================")
    
    manifest = os.path.join(QUAKE_DIR, "manifest")
    icon = os.path.join(QUAKE_DIR, "icon.png")
    autoexec = os.path.join(QUAKE_DIR, "autoexec.cfg")
    out_bpk = os.path.join(QUAKE_DIR, "quake.bpk")
    
    # Arguments for bpkg pack:
    # manifest app-x86_64 app-i686 icon.png id1/pak0.pak=... id1/pak1.pak=... id1/autoexec.cfg=...
    pak0_arg = f"id1/pak0.pak={PAK_FILE}"
    pak1_arg = f"id1/pak1.pak={PAK1_FILE}"
    cfg_arg = f"id1/autoexec.cfg={autoexec}"
    pack_cmd = [
        sys.executable, BPKG_TOOL, "pack", "-o", out_bpk,
        manifest, elf_x86_64, elf_i686, icon, pak0_arg, pak1_arg, cfg_arg
    ]
    
    res = subprocess.run(pack_cmd, capture_output=True, text=True)
    print(res.stdout)
    if res.returncode != 0:
        print("Packaging failed:")
        print(res.stderr)
        sys.exit(1)
        
    print(f"Validating {out_bpk} with bpkg check...")
    check_cmd = [sys.executable, BPKG_TOOL, "check", out_bpk]
    res = subprocess.run(check_cmd, capture_output=True, text=True)
    print(res.stdout)
    if res.returncode != 0:
        print("bpkg check failed:")
        print(res.stderr)
        sys.exit(1)
        
    # Copy to repo directory so web server serves it
    repo_dst = r"C:\BananaOs\repo\quake.bpk"
    shutil.copyfile(out_bpk, repo_dst)
    print(f"Copied {out_bpk} -> {repo_dst} ({os.path.getsize(repo_dst):,} bytes)")
    print("\nQuake package build complete and deployed to HTTP server!")

if __name__ == "__main__":
    main()
