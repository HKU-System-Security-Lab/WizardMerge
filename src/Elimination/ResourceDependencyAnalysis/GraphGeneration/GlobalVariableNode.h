#ifndef GLOBALVARIABLESUMMARY_H
#define GLOBALVARIABLESUMMARY_H

#include "Project.h"
#include "GeneralNode.h"
#include "TypeNode.h"
#include "ModuleSummary.h"

#include <mutex>

class ModuleSummary;
class DependencyGraph;

class GlobalVariableNode : public GeneralNode {
public:
	GlobalVariableNode(DIGlobalVariable *_gV, std::string _name, std::string _source_path,
			 unsigned int _line, size_t _hash_val, ModuleSummary* _ms, DependencyGraph *_dg) : 
        DIgV(_gV), GeneralNode(_name, _source_path, _line, _hash_val, NK_GlobalVar, _dg)
    {
        ms = _ms;
        this->collectType();
    }

    size_t getDescriptionHash();

    std::string debug_print() {
        return "[GlobalVarNode] " + name + " " + source_file_path + " " + to_string(debug_line);
	}

    static bool classof(const GeneralNode *GN) {
		return GN->getKind() == NK_GlobalVar;
	}

    DIGlobalVariable *DIgV;
    std::string name;

    ModuleSummary* ms;

private:
    void collectType();

};

size_t getGlobalVarHash(DIGlobalVariable * DGV);
size_t getGlobalVarNode(GlobalVariable *GV, 
                        ModuleSummary *ms, DependencyGraph *dg);
GlobalVariableNode *createGlobalNode(DIGlobalVariable * DGV, 
            size_t hash_idx, ModuleSummary *ms, DependencyGraph *dg);

// bool globalnode_sorter(GlobalVariableNode *A, GlobalVariableNode *B) {
// 	return A->debug_line < B->debug_line;
// }

#endif