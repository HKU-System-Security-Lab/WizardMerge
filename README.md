# WizardMerge

WizardMerge provides a compromising way to help people with resolving merging conflicts.

## Build 
```shell
git clone https://github.com/HKU-System-Security-Lab/WizardMerge.git
cd WizardMerge
cd src
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ../
make -j8
```

## Test & Evaluation
We have provided some conflict pairs to test WizardMerge in `evaluation/conflict_commit_pairs`.

As an alternative way, if you want to collect the conflict tuple (i.e. (resolved_commit, commit_a, commit_b)), you can change directory to the root of the targeted Git project, and execute:

```shell
python3 find_merge_conflict_tuple.py <tuple_list_path>
```

Please refer to `compile_all_targets.py` to see how to compile and dump definition indicator & LLVM IR for each project automatically.

Please refer to `statical_test.py` to see how we calculate the arguments for `Statistical Confirmation` section.

We also provide part of the ground truth of CR-violated DCBs for your reference.

## Usage

### Use patched git to obtain preliminary code blocks
Compile patched git
```shell
cd third_party/git/git_repo
git apply ../patch.diff
make -j8
```

use patched git
```shell
cd <project_to_merge>
git checkout <commit_a> # using vanilla git to checkout is okay
<patched_git_path> merge --no-commit --no-ff <commit_b>
```
Upon this, a "PreliminaryCodeBlocks" file will be generated under ".git" directory

### Dump LLVM IR and Definition Range
- Apply the patches under `third_party/llvm`. We provided two patches, While `patch.diff` is for LLVM-12.0.1 and `170000_patch.diff` is for LLVM-17.0.0.
- When compile the project, configure the compiler of C and C++ into the patched clang/clang++.
- Configure the compiling optimization to `-O0`.
- Set environmental variables: `export DePart_DumpIR_Out=DumpIR && export Frontend_Mode=DumpRange && export DePart_DumpIR_Out=<IR_dump_dir> && export Frontend_DumpRange_Out=<Range_dump_dir>`.
- Compile the project.

### Use WizardMerge
`/Eliminator -i <IR_and_Range_dir> -j <thread_num> -pa <version_a_source_code> -pb <version_b_source_code> -o <suggestion_output> > <edge_logs>`

- <IR_and_Range_dir>: A single directory contains the IR and definition ranges of the two versions. The directory strcuture should be:
```
<IR_and_Range_dir> ----
                      |
                      ----- IR
                      |      |
                      |      ----- VA
                      |      |
                      |      ----- VB
                      |
                      ----- Range
                      |      |
                      |      ----- VA
                      |      |
                      |      ----- VB
                      |
                      ----- PreliminaryCodeBlocks
```

