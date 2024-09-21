#ifndef UTILS_GRAPHG_H
#define UTILS_GRAPHG_H

#include "Project.h"

#include <iostream>

bool belongsToProject(std::string reqPath, std::string dir);
bool isUserDefinedType(DIType *Ty);
std::string getBasePath(std::string path, std::string dir);

template<typename T>
int isValidDebugInfo(T *D) {
  if (D->getFile() != NULL) {
      if (isa<DISubprogram>(D)) {
        // errs() << project_root_path_VA << " + " << project_root_path_VB  << "\n"
        //        << " " << D->getFilename().str() << " " << D->getDirectory().str() << "\n"
        //        << " - first isSubPath:" << isSubPath(project_root_path_VA, D->getFilename().str(), D->getDirectory().str()) 
        //        << " second isSubPath:" <<  isSubPath(project_root_path_VB, D->getFilename().str(), D->getDirectory().str());
      }
      if (belongsToProject(D->getFilename().str(), D->getDirectory().str())) {
        if (D->getLine() != 0) {
          // std::cerr << "[DEBUG isValidDebugInfo] file and line both found! (line " << D->getLine() << ")\n";
          return 1;
        }
          
        else  {
          // std::cerr << "[DEBUG isValidDebugInfo] only file is found!\n";
          return 0;
        }
          
      }
      // std::cerr << "[DEBUG isValidDebugInfo] file is not included in the project\n";
      // The type is defined outside target project
      return -1;
  }
          
  // std::cerr << "[DEBUG isValidDebugInfo] no file nor line found!\n";
  // not a defined type
  return 0;
}

bool compare_by_range(GeneralNode *A, GeneralNode *B);
bool compare_by_debug_line(GeneralNode *A, GeneralNode *B);

#endif