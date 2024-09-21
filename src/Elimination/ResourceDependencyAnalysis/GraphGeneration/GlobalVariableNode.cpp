#include "GlobalVariableNode.h"
#include "Utils.h"

#include <llvm/IR/GlobalVariable.h>
#include <llvm/IR/DebugInfoMetadata.h>

std::string getName(DIGlobalVariable * DGV) {
    std::string identifier = DGV->getLinkageName().str();
	std::string name = DGV->getDisplayName().str();
    if (!identifier.size() && !name.size()) {
        return "";
    }
    std::string final_name = identifier.size() ? identifier : name;
    return final_name;
}

void GlobalVariableNode::collectType() {
    // errs() << "[DEBUG] GlobalVariableNode::collectType\n";
    ms->fileMapsToNodes[source_file_path].insert(hash_val);
    ms->newNodeToFile[dyn_cast<GeneralNode>(this)] = source_file_path;

    mangled_name = DIgV->getLinkageName().str();

    unordered_set<size_t> results;
	bool is_valuable = getTypeNode(DIgV->getType(), results, ms, dg);
	if (is_valuable) {
		for (auto nextN_idx : results) {
            ms->GlobalVarNodeGraph[hash_val].insert(nextN_idx);
            // ms->GlobalVarNodeGraphhash[hash_val].insert(getTypeHash(nextN->TyPtr));
        }
    }
}

GlobalVariableNode *createGlobalNode(DIGlobalVariable * DGV, 
            size_t hash_idx, ModuleSummary *ms, DependencyGraph *dg) {
    assert(DGV->getLine() && (!DGV->getFilename().empty()) && "[createGlobalNode] Not valid Type in file.");

    std::string identifier = DGV->getLinkageName().str();
	std::string name = DGV->getDisplayName().str();
    // std::string final_name = identifier.size() ? identifier : name;

	std::string base_path = getBasePath(DGV->getFilename().str(), DGV->getDirectory().str());
	
    if (name.size() == 0) {
        /* g_vars.global_print.lock();
        DGV->print(llvm::errs(), nullptr);
        llvm::errs() << "\n" << DGV->getFilename() << " - " << DGV->getLine() << "\n";
        g_vars.global_print.unlock();
        exit(0); */
        return nullptr;
    }

    // if (!identifier.size() && !name.size()) {
    //     return nullptr;
    // }

    GlobalVariableNode *_GN = new GlobalVariableNode(DGV, name, base_path, DGV->getLine(), hash_idx, ms, dg);
    return _GN;
}

size_t getGlobalVarNode(GlobalVariable *GV, 
                        ModuleSummary *ms, DependencyGraph *dg) {
    SmallVector< DIGlobalVariableExpression * > DbgInfoVec;
    GV->getDebugInfo(DbgInfoVec);
    DIGlobalVariable *DIgV = NULL;
    if (!DbgInfoVec.empty()) {
        llvm::DIGlobalVariableExpression* divarExpr = DbgInfoVec[0];
        DIgV = divarExpr->getVariable();
    }
    if (DIgV == NULL || isValidDebugInfo(DIgV) != 1)
        return 0;

    // TODO:
    // What if a gv does not have name?

    size_t node_idx = getGlobalVarHash(DIgV);

    if (ms->GlobalVarNodeList.count(node_idx)) {
        // GlobalVariableNode *ret = ms->GlobalVarNodeList[node_idx];
        return node_idx;
    }
    GlobalVariableNode *new_GN = createGlobalNode(DIgV, node_idx, ms, dg);
    if (new_GN == nullptr) return 0;
    ms->GlobalVarNodeList[node_idx] = new_GN;
    ms->GlobalVarNodeToHash[new_GN] = node_idx;
    // ms->newGVNodes[node_idx] = new_GN;
    // dg->mtx_access.lock();
    // printf("[idx = %d getGlobalVarNode] create new node ", ms->thread_idx);
    // new_GN->debug_print();
    // for (auto *TN : ms->GlobalVarNodeGraph[new_GN]) {
    //     printf("        [SubTN] %ld: ", getTypeHash(TN->TyPtr));
    //     TN->debug_print();
    // }
    // dg->mtx_access.unlock();
    return node_idx;
}

// GlobalVariableNode *getGlobalVarNode(DIGlobalVariable *DIGV) {
//     if (!DIGV || isValidDebugInfo(DIGV) != 1)
//         return NULL;

//     size_t node_idx = getGlobalVarHash(DIGV);
//     if (dg->GlobalVarNodeList.count(node_idx))
//         return dg->GlobalVarNodeList[node_idx];

//     GlobalVariableNode *new_GN = new GlobalVariableNode(DIDIGVgV);
//     dg->GlobalVarNodeList[node_idx] = new_GN;
//     return new_GN;
// }

size_t GlobalVariableNode::getDescriptionHash() {
    return (size_t) llvm::hash_combine(source_file_path, getName(DIgV), getKind());
}

size_t getGlobalVarHash(DIGlobalVariable * DGV) {
    assert(DGV->getLine() && (!DGV->getFilename().empty()) && "[getGlobalVarHash] Not valid Type in file.");

	std::string base_path = getBasePath(DGV->getFilename().str(), DGV->getDirectory().str());
	std::string final_name = getName(DGV);

	return (size_t) llvm::hash_combine(base_path, DGV->getLine(), final_name);
}
