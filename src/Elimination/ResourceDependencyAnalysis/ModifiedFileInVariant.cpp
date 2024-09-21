#include "ProjectRepresentation.h"
#include <llvm/IR/DebugInfo.h>

#include <algorithm>
#define INF 0x3f3f3f3f

using namespace llvm;

bool ModifiedFileInVariant::readIRFromFile(std::string IRFilePath) {
    _M = parseIRFile(IRFilePath, smd, ctx);
    if (!_M) return false;
    return true;
}
