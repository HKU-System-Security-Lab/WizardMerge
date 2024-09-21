#include "Utils.h"
#include "GeneralNode.h"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugInfo.h>

#include <stack>
#include <sstream>
#include <vector>
#include <filesystem>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <string>
#include <fstream>
#include <mutex>

thread_local std::unordered_map<std::string, bool> belongsToProject_cache;
thread_local std::unordered_map<std::string, std::string> basePath_cache;

std::string getAbsolutePath(const std::string path) {
    std::stack<std::string> stack;
    std::string result;

    std::stringstream ss(path);
    std::string directory;

    while (std::getline(ss, directory, '/')) {
        if (directory == "" || directory == ".") {
            continue;
        } else if (directory == "..") {
            if (!stack.empty()) stack.pop();
        } else if (directory == "...") {
            for (int i = 0; i < 2; i++)
                if (!stack.empty()) stack.pop();
        } else {
            stack.push(directory);
        }
    }

    std::vector<std::string> directories;
    while (!stack.empty()) {
        directories.push_back(stack.top());
        stack.pop();
    }

    for (int i = directories.size() - 1; i >= 0; --i)
        result += '/' + directories[i];

    return result.empty() ? "/" : result;
}

bool belongsToProject(std::string reqPath, std::string dir) {
    // path_debug_mtx.lock();
    std::string project_root_path = g_vars.current_in_va ? g_vars.project_root_path_VA : g_vars.project_root_path_VB;
    std::string ctx_project_root_path = g_vars.current_in_va ? g_vars.ctx_project_root_path_va : g_vars.ctx_project_root_path_vb;

    /*
      If "dir" is passed, we concat it with the base path.
    */
    if (dir.length() != 0)
      reqPath = dir + "/" + reqPath;

    /*
      As the full_path may be a relative path, we need to transfer it into an absolute one
    */
    std::string normalized_reqPath = getAbsolutePath(reqPath);

    /*
      Fast path: use cache
    */
    if (belongsToProject_cache.count(normalized_reqPath))
      return belongsToProject_cache[normalized_reqPath];

    /*
      If no repo prefix is found, the path is not in the current project.
    */
    if (normalized_reqPath.find(project_root_path) == std::string::npos) {
      belongsToProject_cache[normalized_reqPath] = false;
      return false;
    }

    /*
      Delete the prefix as we only care about the base name.
    */
    std::string base_reqPath = normalized_reqPath;
    base_reqPath.erase(0, project_root_path.length() + 1);

    belongsToProject_cache[normalized_reqPath] = g_vars.diff_file_set.count(base_reqPath);
    basePath_cache[normalized_reqPath] = base_reqPath;
    return belongsToProject_cache[normalized_reqPath];
}

std::string getBasePath(std::string reqPath, std::string dir) {
    std::string project_root_path = g_vars.current_in_va ? g_vars.project_root_path_VA : g_vars.project_root_path_VB;
    std::string ctx_project_root_path = g_vars.current_in_va ? g_vars.ctx_project_root_path_va : g_vars.ctx_project_root_path_vb;

    if (dir.length() != 0)
      reqPath = dir + "/" + reqPath;
    std::string normalized_reqPath = getAbsolutePath(reqPath);
    
    if (normalized_reqPath.find(project_root_path) == std::string::npos) {
      assert(false && "Never should reach here");
      exit(-1);
    }

    assert(basePath_cache.count(normalized_reqPath) && "reqPath not found in basePath_cache");
    return basePath_cache[normalized_reqPath];
}

bool isUserDefinedType(DIType *Ty) {
  if (isa<DICompositeType>(Ty) || isa<DIDerivedType>(Ty))
    return Ty->getFile() && !Ty->getName().empty() 
            && Ty->getLine() && !Ty->getFilename().empty()
             && belongsToProject(Ty->getFilename().str(), Ty->getDirectory().str());
  return false;
}

bool compare_by_range(GeneralNode *A, GeneralNode *B) {
  return A->start_line == B->start_line 
      ? A->end_line < B->end_line 
      : A->start_line < B->end_line;
}

bool compare_by_debug_line(GeneralNode *A, GeneralNode *B) {
  if (A->debug_line == B->debug_line) {
    if (A->name.size() == B->name.size())
      return A->name < B->name;
    return A->name.size() < B->name.size();
  }
  return A->debug_line < B->debug_line;
}