import os, sys
import subprocess

def process_directly_merged_file(file1, file2):
    ret = []
    with open(file1, 'r') as f:
        base_rep = {line.split(' --- ')[0]: line.split(' --- ')[1].strip() for line in f.readlines()}

    with open(file2, 'r') as f:
        for line in f.readlines():
            base_name, l1, l2 = line.strip().split(', ')
            if base_name in base_rep:
                apply_type = '+' if base_rep[base_name] == 'A' else '-'
                ret.append(f'@ {base_name}, {l1}, {l2}')
                ret.append(f'{apply_type} 1, {l1}, = ,1, {l2}')
    return ret

def git_merge(commit, work_dir, git_path = None):
    if git_path is None:
        git_path = "git"
    print(" ".join([git_path, 'merge', '--quiet', '--no-commit', '--no-ff', commit]))
    out = subprocess.run(
        [git_path, 'merge', '--quiet', '--no-commit', '--no-ff', commit],
        stdout=subprocess.DEVNULL,
        cwd=work_dir
    )
    return out

def git_merge_abort(work_dir):
    subprocess.run(
        ['git', 'merge', '--abort'],
        check=True,
        cwd=work_dir
    )

def git_checkout(commit, work_dir):
    print(" ".join(['git', 'checkout', commit]))
    out = subprocess.run(
        ['git', 'checkout', commit],
        check=True,
        cwd=work_dir,
        stderr=subprocess.PIPE
    )
    
def run_get_diff_lines(c1, c2, work_dir):
    script_dir = os.path.dirname(os.path.abspath(__file__))
    script_path = os.path.join(script_dir, "get_diff_lines.sh")
    dump_path = os.path.join(work_dir, ".git", "diff_file_lines")
    out = subprocess.run(
        [script_path, c1, c2, dump_path],
        check=True,
        cwd=work_dir,
        stderr=subprocess.PIPE
    )

def perform_merge(c1, c2, work_dir, git_path, out_path):
    
    # Generate initial PreliminaryCodeBlocks, directly merged file list and their file length
    os.environ["GIT_DMERGED_FILE_LIST"] = os.path.join(work_dir, ".git", "dmerged_list")
    
    dmerged_list_path = os.path.join(work_dir, ".git", "dmerged_list")
    line_list_path = os.path.join(work_dir, ".git", "diff_file_lines")
    init_PCB_path = os.path.join(work_dir, ".git", "PreliminaryCodeBlocks")
    
    if (os.path.exists(dmerged_list_path)):
        os.remove(dmerged_list_path)
    if (os.path.exists(line_list_path)):
        os.remove(line_list_path)
    if (os.path.exists(init_PCB_path)):
        os.remove(init_PCB_path) 
    
    
    git_checkout(c1, work_dir)
    git_merge(c2, work_dir, git_path)
    git_merge_abort(work_dir)
    run_get_diff_lines(c1, c2, work_dir)
    dmerged_list_str_list = process_directly_merged_file(dmerged_list_path, line_list_path)
   
    dmerged_list_str_list = [output + "\n" for output in dmerged_list_str_list]
    
    prev_lines = None
    
    with open(init_PCB_path, "r") as f:
        prev_lines = f.readlines()
        
    final_output = dmerged_list_str_list + prev_lines
    with open(out_path, "w") as f:
        f.writelines(final_output)
        
def get_DCB_for_conflicts(dataset_path, work_dir, git_path):
    dataset_bases = os.listdir(dataset_path)
    for base in dataset_bases:
        if not os.path.isdir(os.path.join(dataset_path, base)):
            continue
        c1 = base.split("_")[0]
        c2 = base.split("_")[1]
        output_path = os.path.join(dataset_path, base, "PreliminaryCodeBlocks")
        perform_merge(c1, c2, work_dir, git_path, output_path)
    
    
if __name__ == "__main__":
    get_DCB_for_conflicts(sys.argv[1], sys.argv[2], sys.argv[3])
    # perform_merge(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])

    
    