#ifndef __GLOBALVARS_H
#define __GLOBALVARS_H

#include <unordered_map>
#include <mutex>

class GeneralNode;

typedef struct GlobalVars {

    // Whether we are handling data in VA currently.
    bool current_in_va;

    // All files related to git diff
    std::unordered_set<std::string> diff_file_set;

    // The root path of the project.
    // They should be consistent with the IR metadata and their file names.
    std::string project_root_path_VA;
    std::string project_root_path_VB;

    // This is a trick to locate the files in your current working context.
    //
    // For example, users may dump IR & Range on one server, but run MvCDA
    // on another.
    //
    // In this case, belongsToProject function may return false because MvCDA 
    // cannot find a corresponding file in the current working context.
    std::string ctx_project_root_path_va;
    std::string ctx_project_root_path_vb;

    // Total number of IR files in VA and VB.
    int va_ir_num, vb_ir_num;

    std::mutex global_print;

    bool use_prototype_rep;
} GlobalVars_t;

#endif