import os, sys, getopt
import shutil
import subprocess

pre_cmd = {
    "linux": ["make", "allyesconfig"],
    "firefox": ["./mach", "clobber"],
    "php": ["./buildconf", "&&", "./configure", "--enable-debug"],
    # You will need to set customized ssl and boost library paths by replacing "xx" to correct path
    "mysql": ["cmake", "-DCMAKE_C_FLAGS=-g -O0", "-DCMAKE_CXX_FLAGS=-g -O0", "-DDOWNLOAD_BOOST_TIMEOUT=1200", "-DWITH_SSL=xx", "-DDOWNLOAD_BOOST=1", "-DWITH_BOOST=xx", "-DWITH_DEBUG=1", ".."],
    "llvm": ["cmake", "-DCMAKE_BUILD_TYPE=Debug", "-DLLVM_ENABLE_ASSERTIONS=OFF", "-DLLVM_INCLUDE_TESTS=OFF", "-DLLVM_ENABLE_RUNTIMES=all", "-DLLVM_ENABLE_PROJECTS=all", "../llvm"]
}

compile_cmd = {
    "linux": ["make", "LLVM=1", "-j32"],
    "firefox": ["./mach", "build"],
    "php": ["make", "-j32"],
    "mysql": ["make", "-j32"],
    "llvm": ["make", "-j32"]
}

after_cmd = {
    "linux": ["make", "clean"],
    "firefox": ["rm", "./mozconfig"],
    "php": ["make", "clean"],
    "mysql": ["rm", "-rf", "./*"],
    "llvm": ["rm", "-rf", "./*"]
}

def set_envs(ir_root, range_root, is_va):
    base = ""
    if is_va:
        base = "VA"
    else:
        base = "VB"

    ir_path = os.path.join(ir_root, base)
    range_root = os.path.join(range_root, base)

    os.environ["DePart_DumpIR_Out"] = ir_path
    os.environ["Frontend_DumpRange_Out"] = range_root

def git_checkout(commit):
    # print(os.environ["PATH"])
    # os.system(" ".join(['git', 'checkout', commit]))
    # exit(0)
    out = subprocess.run(
        ['git', 'checkout', commit],
        check=True,
        stderr=subprocess.PIPE
    )
    # print(stderr)

def do_compile(project_root_path):
    target_project = sys.argv[2]
    build_path = os.path.join(project_root_path, "build")
    cwd_path = project_root_path

    # generate build directory
    if target_project in ["llvm", "mysql"]:
        if not os.path.exists(build_path):
            os.mkdir(build_path)
        cwd_path = build_path
    
    # Pre run
    p = subprocess.Popen(pre_cmd[target_project], cwd=cwd_path, stderr=subprocess.PIPE)
    stderr = p.communicate()
    p.wait()
    
    if target_project == "linux":
        os.system("sed -i '/CONFIG_KGDB=y/d' ./.config")
        os.system("sed -i '/CONFIG_UBSAN=y/d' ./.config")
        os.system("sed -i '/CONFIG_KCSAN=y/d' ./.config")

    # Compile
    p = subprocess.Popen(compile_cmd[target_project], cwd=cwd_path, stderr=subprocess.PIPE)
    _, stderr = p.communicate()
    p.wait()
    if p.returncode != 0:
        print(' '.join(compile_cmd))
        print("Error: ", stderr)
        # exit(-1)
        return stderr

        
    # after compilation
    out = subprocess.run(after_cmd[target_project], cwd=cwd_path, check=True)

    return None
    
def run_each(commit, ir_root, range_root, project_root_path, is_va):

    git_checkout(commit)

    # compile
    set_envs(ir_root, range_root, is_va)
    return do_compile(project_root_path)

def conflicts_retrieve(conflict_pair_path):
    list_len = 0
    ret_list = []
    lines = None
    c1, c2 = None, None
    with open(conflict_pair_path, "r") as f:
        lines = f.readlines()

    for line in lines:
        if len(line) < 3:
            continue
        
        c1 = line.split(" ")[0]
        c2 = line.split(" ")[1][:-1]
        
        ret_list.append((c1, c2))
    return ret_list

def compile_all():
    conflict_pair_path = int(sys.argv[1])
    target_project = sys.argv[2]
    target_output = sys.argv[3]
    project_va_path = sys.argv[4]
    project_vb_path = sys.argv[5]
    clang_bin_dir = sys.argv[6]

    if target_project not in ["linux", "firefox", "php", "mysql", "llvm"]:
        exit(-1)

    if not os.path.exists(project_va_path) or not os.path.exists(project_vb_path):
        exit(-1)
    
    if not os.path.exists(clang_bin_dir):
        exit(-1)
    

    conflicts_pairs = conflicts_retrieve(conflict_pair_path)

    # Setting global environmental variables
    os.environ["PATH"] = clang_bin_dir + ":" + os.environ["PATH"]
    os.environ["KCFLAGS"] = "-Og -g -Wno-error"
    os.environ["KCPPFLAGS"] = "-Og -g -Wno-error"
    os.environ["DePart_Mode"] = "DumpIR"
    os.environ["Frontend_Mode"] = "DumpRange"
    os.environ["CFLAGS"] = "-Og -g -Wno-error"
    os.environ["CXXFLAGS"] = "-Og -g -Wno-error"
    os.environ["CC"] = os.path.join(clang_bin_dir, "clang")
    os.environ["CXX"] = os.path.join(clang_bin_dir, "clang++")

    dump_path = target_output
    
    os.chdir(project_va_path)

    for c1, c2 in conflicts_pairs:
        new_dir = os.path.join(dump_path, c1 + "_" + c2)
        ir_dump_root_path = os.path.join(new_dir, "IR")
        range_dump_root_path = os.path.join(new_dir, "Range")

        if not os.path.exists(new_dir):
            os.mkdir(new_dir)

        if not os.path.exists(ir_dump_root_path):
            os.mkdir(ir_dump_root_path)
        if not os.path.exists(ir_dump_root_path + "/VA"):    
            os.mkdir(ir_dump_root_path + "/VA")
        if not os.path.exists(ir_dump_root_path + "/VB"):    
            os.mkdir(ir_dump_root_path + "/VB")

        if not os.path.exists(range_dump_root_path):    
            os.mkdir(range_dump_root_path)
        if not os.path.exists(range_dump_root_path + "/VA"):    
            os.mkdir(range_dump_root_path + "/VA")
        if not os.path.exists(range_dump_root_path + "/VB"):    
            os.mkdir(range_dump_root_path + "/VB")

        os.chdir(project_va_path)
        result = run_each(c1, ir_dump_root_path, range_dump_root_path, project_va_path, True)

        if result is not None:
            err_msg_path = os.path.join(new_dir, "err_msg_A")
            with open(err_msg_path, "wb") as f:
                f.write(result)

        subprocess.run(['touch', new_dir + '/finish_VA'], check=True)


        os.chdir(project_vb_path)
        result = run_each(c1, ir_dump_root_path, range_dump_root_path, project_vb_path, False)

        if result is not None:
            err_msg_path = os.path.join(new_dir, "err_msg_B")
            with open(err_msg_path, "wb") as f:
                f.write(result)

        subprocess.run(['touch', new_dir + '/finish_VB'], check=True)

if __name__ == "__main__":
    compile_all()
