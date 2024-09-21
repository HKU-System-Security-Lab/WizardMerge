import os
import sys
import json
import argparse
import subprocess

PATCHED_GIT_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "third_party", "git", "git_repo", "git")
result = subprocess.run(["git", "rev-parse", "HEAD"], stdout=subprocess.PIPE)
CURRENT_COMMIT_ID = result.stdout.decode().strip()

def parse_cmd_args():
    parser = argparse.ArgumentParser(description='Help git merge with generating PCB list.')
    parser.add_argument("target_commit_id", help="target commit id")
    return parser.parse_args()

def check_currently_in_repo():
    return os.path.exists(".git")

def check_patched_git_exists():
    return os.path.exists(PATCHED_GIT_PATH)

def check_runtime_env():
    if not check_currently_in_repo():
        print("Not in a Git repo.")
        return False
    
    if not check_patched_git_exists():
        print("Patched `git` under {} is not found.".format(os.path.abspath(PATCHED_GIT_PATH)))
        return False
    
    return True

def do_merge(c_id):
    # Merge with given commit id
    os.system(' '.join([PATCHED_GIT_PATH, 'merge', '--quiet', '--no-commit', '--no-ff', c_id]))
    
    # Recover
    subprocess.run(['git', 'merge', '--abort'], check=True)
    
def do_checkout(target_id):
    subprocess.run(['git', 'checkout', target_id], check=True)

def get_file_line_number(rela_path):
    result = subprocess.run(["wc", "-l", rela_path], stdout=subprocess.PIPE)
    output = int(result.stdout.decode().strip().split(" ")[0])
    return output

"""
{
    "file_name": str, {
        "va_line_number": int,
        "vb_line_number": int
        "DCBs": [(apply_type va_start, va_len, vb_start, vb_len), ...]
    },
    ...
}
{
    "file_name": str {
        "apply_type": int,
        "va_line_number": int,
        "vb_line_number": int
    },
    ...
}
"""

def collect_fast_merged_files():
    if not os.path.exists(".git/PreliminaryCodeBlocks"):
        print("'.git/PreliminaryCodeBlocks' not found.")
        exit(1)
    
    reserved_results = {}
    fast_merged_list = {}
    
    with open(".git/PreliminaryCodeBlocks", "r") as f:
        lines = [line.rstrip() for line in f.readlines()]
        va_total_line, vb_total_line = 0, 0
        cur_file_name = ""
        for line in lines:
            if line.startswith("@"):
                cur_file_name = line.split(", ")[0].strip().split()[1]
                va_total_line = int(line.split(", ")[1].strip())
                vb_total_line = int(line.split(", ")[2].strip())
                if va_total_line < 0 and vb_total_line < 0:
                    fast_merged_list[cur_file_name] = {}
                    fast_merged_list[cur_file_name]["apply_type"] = 0 if va_total_line == -2 else 1
                    fast_merged_list[cur_file_name]["va_line_number"], fast_merged_list[cur_file_name]["vb_line_number"] = 0, 0
                else:
                    reserved_results[cur_file_name] = {}
                    reserved_results[cur_file_name]["va_line_number"] = va_total_line
                    reserved_results[cur_file_name]["vb_line_number"] = vb_total_line
                    reserved_results[cur_file_name]["DCBs"] = []
            else:
                diff_type = line.split(" ")[0]
                va_start = int(line.split(" ")[1].split(",")[0])
                vb_start = int(line.split("= ,")[1].split(",")[0])
                va_len = int(line.split(" ")[2].split(",")[0])
                vb_len = int(line.split(" ")[-1])
                
                # 0 = apply A
                # 1 = apply B
                # 2 = conflict
                apply_type = -1
                if diff_type.startswith("+"):
                    apply_type = 0
                if diff_type.startswith("-"):
                    apply_type = 1
                if diff_type.startswith("#"):
                    apply_type = 2
                
                reserved_results[cur_file_name]["DCBs"].append((
                    apply_type,
                    va_start, va_len,
                    vb_start, vb_len
                ))
        
    return reserved_results, fast_merged_list

def fill_fast_merged_file_info(target_commit_id, fast_merged_list):
    for file in fast_merged_list.keys():
        fast_merged_list[file]["va_line_number"] = get_file_line_number(file)
        
    do_checkout(target_commit_id)
    
    for file in fast_merged_list.keys():
        fast_merged_list[file]["vb_line_number"] = get_file_line_number(file)
        
    do_checkout(CURRENT_COMMIT_ID)
        
def do_diff(target_commit_id, file):
    result = subprocess.run(["git", "diff", "--no-color", "--unified=0", target_commit_id, file], stdout=subprocess.PIPE)
    return result.stdout

def parse_pcb_from_diff(diff_bytes, apply_type):
    lines = diff_bytes.split(b"\n")
    pcbs = []
    
    for line in lines:
        # New PCB begins
        if line.startswith(b"@@"):
            line = line.decode()
            
            # "xx(,xx) xx(,xx)"
            line = line.split(" @@ ")[0].split("@@ ")[1]
            
            # "xx(,xx)" and "xx(,xx)"
            part1, part2 = line.split(" ")[0], line.split(" ")[1]
            
            va_start, vb_start, va_len, vb_len = 0, 0, 0, 0
            va_find_len, vb_find_len = False, False
            
            # get start and len
            vb_start = int(part1.split(",")[0].split("-")[1])
            if len(part1.split(",")) == 2:
                vb_find_len = True
                vb_len = int(part1.split(",")[1])
            else:
                vb_len = 0
            
            va_start = int(part2.split(",")[0].split("+")[1])
            if len(part2.split(",")) == 2:
                va_find_len = True
                va_len = int(part2.split(",")[1])
            else:
                va_len = 0
            
            if not va_find_len and vb_find_len and vb_len == 0:
                va_len = 1
            
            if not vb_find_len and va_find_len and va_len == 0:
                vb_len = 1
                
            if not va_find_len and not vb_find_len:
                va_len = vb_len = 1
                
            if va_len == 0 and vb_len == 0:
                print((apply_type, va_start, va_len, vb_start, vb_len))
            
            if va_start == 0 and va_len == 0:
                continue
            if vb_start == 0 and vb_len == 0:
                continue
            pcbs.append((apply_type, va_start, va_len, vb_start, vb_len))
                
    return pcbs       
       

def regenerate_pcb_list(reserved_results, fast_merged_list, target_commit_id):
    for file in fast_merged_list.keys():
        diff_out = do_diff(target_commit_id, file)
        pcbs = parse_pcb_from_diff(diff_out, fast_merged_list[file]["apply_type"])
        reserved_results[file] = {}
        reserved_results[file]["va_line_number"] = fast_merged_list[file]["va_line_number"]
        reserved_results[file]["vb_line_number"] = fast_merged_list[file]["vb_line_number"]
        reserved_results[file]["DCBs"] = pcbs
        
    # Output back to the file
    with open(".git/PreliminaryCodeBlocks", "w") as f:
        lines = []
        for file in reserved_results.keys():
            lines.append("@ {}, {}, {}\n".format(file, 
                reserved_results[file]["va_line_number"], reserved_results[file]["vb_line_number"]))
            for apply_type, va_start, va_len, vb_start, vb_len in reserved_results[file]["DCBs"]:
                apply_flag = ""
                if apply_type == 0:
                    apply_flag = "+"
                if apply_type == 1:
                    apply_flag = "-"
                if apply_type == 2:
                    apply_flag = "#"
                lines.append("{} {}, {}, = ,{}, {}\n".format(
                    apply_flag,
                    va_start, va_len,
                    vb_start, vb_len
                ))
        for line in lines:
            f.write(line)

def main():
    args = parse_cmd_args()
    
    if not check_runtime_env():
        exit(1)
        
    do_merge(args.target_commit_id)
    reserved_results, fast_merged_list = collect_fast_merged_files()
    fill_fast_merged_file_info(args.target_commit_id, fast_merged_list)
    regenerate_pcb_list(reserved_results, fast_merged_list, args.target_commit_id)

if __name__ == "__main__":
    main()
