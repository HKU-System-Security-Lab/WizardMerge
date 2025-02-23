# https://stackoverflow.com/questions/55817243/does-git-keep-a-record-of-past-merge-conflicts
import subprocess
import shutil
import sys, os

idx = 0
round = 0

C_CPP = 0
JAVA = 1
PYTHON = 2
JAVASCRIPT = 3
CSHARP = 4

ACCEPT_LANGUAGE = C_CPP
if len(sys.argv) >= 4:
    if sys.argv[3] == "java":
        ACCEPT_LANGUAGE = JAVA
        os.environ["GIT_PCB_LAN"] = "java"
    elif sys.argv[3] == "python":
        ACCEPT_LANGUAGE = PYTHON
        os.environ["GIT_PCB_LAN"] = "python"
    elif sys.argv[3] == "javascript":
        ACCEPT_LANGUAGE = JAVASCRIPT
        os.environ["GIT_PCB_LAN"] = "javascript"
    elif sys.argv[3] == "c#":
        ACCEPT_LANGUAGE = CSHARP
        os.environ["GIT_PCB_LAN"] = "csharp"
        
def should_be_considered(file):
    if ACCEPT_LANGUAGE == C_CPP:
        return file.endswith(".cc") or file.endswith(".cpp") or file.endswith(".hh") or file.endswith(".C") or file.endswith(".c") or file.endswith(".h") or file.endswith(".hpp") or file.endswith(".cxx") or file.endswith(".c++")
    if ACCEPT_LANGUAGE == JAVA:
        return  file.endswith(".java")
    if ACCEPT_LANGUAGE == PYTHON:
        return  file.endswith(".py")
    if ACCEPT_LANGUAGE == JAVASCRIPT:
        return  file.endswith(".js")
    if ACCEPT_LANGUAGE == CSHARP:
        return  file.endswith(".cs")

def list_merges():
    print(' '.join(['git', 'rev-list', '--parents', '--min-parents=2', '--all']))
    # exit(0)
    proc = subprocess.run(
        ['git', 'rev-list', '--parents', '--min-parents=2', '--all'],
        stdout=subprocess.PIPE,
        check=True,
        encoding='ASCII',
    )
    merges = []
    for line in proc.stdout.splitlines():
        fields = line.split()
        merges.append((fields[0], fields[1:]))
    # print('Found {} merges'.format(len(merges)), file=sys.stderr)
    return merges

def analyze_merge(commit, parents, store_path, patched_git, start_point=0):
    global idx, round
    print("======================= starting round %d with commit %s ===============================" % (round, commit))
    # print(parents)
    print(' '.join(['git', 'checkout', '--detach', parents[start_point]]))
    out = subprocess.run(
        ['git', 'checkout', '--detach', parents[start_point]],
        check=True,
        stderr=subprocess.PIPE
    )
    for line in out.stderr.splitlines():
        line_str = line.decode()
        if "HEAD is now at" in line_str:
            if "placeholder" in line_str or "Placeholder" in line_str:
                print("placeholder, skip.....")
                return [], []
    idx = 0
    
    ret_conflict = []
    ret_no_conflict = []
    
    
    file_line_number_map = {}

    for cur_parent in parents[start_point+1:]:
        # print("Run \"git merge-base %s %s\" to get the common ancestor" % (parents[start_point], cur_parent))
        out = subprocess.run(
            ['git', 'merge-base', parents[start_point], cur_parent],
            stdout=subprocess.DEVNULL,
        )
        if out.returncode:
            # not related history
            continue
        
        
        print(' '.join([patched_git, 'merge', '--quiet', '--no-commit', '--no-ff', cur_parent]))
        out = subprocess.run(
            [patched_git, 'merge', '--quiet', '--no-commit', '--no-ff', cur_parent],
            stdout=subprocess.DEVNULL,
        )
        
        if ACCEPT_LANGUAGE == C_CPP:
            diff_out = subprocess.run(['git diff %s %s -- \'*.c\' \'*.h\' \'*.hh\' \'*.hpp\' \'*.cc\' \'*.cpp\' \'*.cxx\'\'*.c++\' \'*.C\'' 
                 % (parents[start_point], cur_parent)], 
                shell = True,
                stdout=subprocess.PIPE)
        if ACCEPT_LANGUAGE == JAVA:
            diff_out = subprocess.run(['git diff %s %s -- \'*.java\'' 
                 % (parents[start_point], cur_parent)], 
                shell = True,
                stdout=subprocess.PIPE)
        if ACCEPT_LANGUAGE == PYTHON:
            diff_out = subprocess.run(['git diff %s %s -- \'*.py\'' 
                 % (parents[start_point], cur_parent)], 
                shell = True,
                stdout=subprocess.PIPE)
        if ACCEPT_LANGUAGE == JAVASCRIPT:
           diff_out = subprocess.run(['git diff %s %s -- \'*.js\'' 
                 % (parents[start_point], cur_parent)], 
                shell = True,
                stdout=subprocess.PIPE)
        if ACCEPT_LANGUAGE == CSHARP:
            diff_out = subprocess.run(['git diff %s %s -- \'*.cs\'' 
                 % (parents[start_point], cur_parent)], 
                shell = True,
                stdout=subprocess.PIPE)
        # diff_out = subprocess.run(['git diff %s %s -- \'*.c\' \'*.h\' \'*.hh\' \'*.hpp\' \'*.cc\' \'*.cpp\' \'*.cxx\'\'*.c++\' \'*.C\'' 
        #                 % (parents[start_point], cur_parent)], 
        #                shell = True,
        #                stdout=subprocess.PIPE)
    
        line_num = 0    
        for line in diff_out.stdout.splitlines():
            # print("$$$ ", line)
            line_num += 1
        if line_num == 0:
            subprocess.run(
                ['git', 'merge', '--abort'],
                check=True
            )
            continue
        
        cur_pair_store_path = os.path.join(store_path, parents[start_point] + "_" + cur_parent)
        cur_PCB_store_path = os.path.join(cur_pair_store_path, "PreliminaryCodeBlocks")
        if out.returncode:
            # Merge conflicts
            # continue
            print("Conflicts between %s and %s" % (parents[start_point], cur_parent))
            out = subprocess.run(
                ['git', 'status', '--porcelain'],
                stdout=subprocess.PIPE,
                check=True,
                encoding='ASCII',
            )
            
            found_c_file = False
            for line in out.stdout.splitlines():
                if line.startswith("UU"):
                    if should_be_considered(line[3:]):
                    # if line[3:].endswith(".cc") or line[3:].endswith(".hh") or line[3:].endswith(".cpp") or line[3:].endswith(".h") or line[3:].endswith(".hpp") or line[3:].endswith(".c") or line[3:].endswith(".C") or line[3:].endswith(".cxx") or line[3:].endswith(".c++"):
                        found_c_file = True
                        break
                        # print("    File: {}".format(line[3:]))
                        # subprocess.run(['git diff %s > %s' % (line[3:], os.path.join(store_path, "%d_%d.diff" % (round, idx)))], shell=True)
                        # idx += 1
            if found_c_file:
                # if os.path.exists("./.git/PreliminaryCodeBlocks"):
                #     if not os.path.exists(cur_pair_store_path):
                #         os.mkdir(cur_pair_store_path)
                #     shutil.copyfile("./.git/PreliminaryCodeBlocks", cur_PCB_store_path)
                ret_conflict.append((commit, parents[start_point], cur_parent))
        # else:
        #     if os.path.exists("./.git/PreliminaryCodeBlocks"):
        #         if not os.path.exists(cur_pair_store_path):
        #             os.mkdir(cur_pair_store_path)
        #         shutil.copyfile("./.git/PreliminaryCodeBlocks", cur_PCB_store_path)
        #         ret_no_conflict.append((commit, parents[start_point], cur_parent))
            
        subprocess.run(
                ['git', 'merge', '--abort'],
            )
        # exit(0)
            
    round += 1
    return ret_conflict, ret_no_conflict

def main():
    if not os.path.exists(sys.argv[1]):
        os.mkdir(sys.argv[1])
    merges = list_merges()
    no_conflict_datasets = []
    conflict_datasets = []
    # file_init_no_conflict = False
    file_init_conflict = False
    # print(merges)
    
    # no_conflict_list_path = os.path.join(sys.argv[1], "no_conflict_pair_list")
    conflict_list_path = os.path.join(sys.argv[1], "conflict_pair_list")
    
    for commit, parents in merges:
        round_conflict_datasets, round_no_conflict_datasets = analyze_merge(commit, parents, sys.argv[1], sys.argv[2])
        
        # if not file_init_no_conflict:
        #     with open(no_conflict_list_path, "w") as f:
        #         for result, c1, c2 in round_no_conflict_datasets:
        #             f.write(result + " " + c1 + " " + c2 + "\n")
        #     file_init_no_conflict = True
        # else:
        #     with open(no_conflict_list_path, "a") as f:
        #         for result, c1, c2 in round_no_conflict_datasets:
        #             f.write(result + " " + c1 + " " + c2 + "\n")
        
        if not file_init_conflict:
            with open(conflict_list_path, "w") as f:
                for result, c1, c2 in round_conflict_datasets:
                    f.write(result + " " + c1 + " " + c2 + "\n") 
            file_init_conflict = True
        else:
            with open(conflict_list_path, "a") as f:
                for result, c1, c2 in round_conflict_datasets:
                    f.write(result + " " + c1 + " " + c2 + "\n") 
        
        # no_conflict_datasets += round_no_conflict_datasets
        conflict_datasets += round_conflict_datasets 

if __name__ == "__main__":
    main()
