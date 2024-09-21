#ifndef FUNCTIONNODE_H
#define FUNCTIONNODE_H

#include "Project.h"
#include "GlobalVariableNode.h"
#include "TypeNode.h"
#include "GeneralNode.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/SmallPtrSet.h"
#include <mutex>

class ModuleSummary;
class DependencyGraph;


// Different the DebugInfoFinder class in LLVM,
// this helper is just for parse the DIType used in 
// a certain function (i.e., DISubProgram) 
class DebugInfoHelper {
public:
	DebugInfoHelper(Function *func) {
		for (auto &BB : *func)
			for (auto &I : BB)
				processInstruction(I);
	}

	DebugInfoHelper(Function *func, std::string name) {
		for (auto &BB : *func)
			for (auto &I : BB) {
				processInstruction(I);
				if (name == "_ZN7mozilla17EventStateManager10WheelPrefs11GetIndexForEPKNS_16WidgetWheelEventE") {
					errs() << I << "\n";
					for (DIType *Ty : types) {
						errs() << "[FROM Helper] body subTy = " << *Ty << "\n";
					}
					errs() << "====================================================\n";
				}
			}
				
	}
	SmallVector<DIType *, 8> types;

private:
	void processInstruction(Instruction &Inst);
	void processScope(DIScope *Scope);
	void processType(DIType *Ty);
	void processLocation(DILocation *Loc);
	bool addType(DIType *DT);
	SmallPtrSet<const MDNode *, 32> NodesSeen;
};

class FunctionNode : public GeneralNode {
public:
	FunctionNode(Function *_func, std::string _name, std::string _source_path,
			 unsigned int _line, size_t _hash_val, ModuleSummary* _ms, DependencyGraph *_dg) : 
		GeneralNode(_name, _source_path, _line, _hash_val, NK_Function, _dg)
	{
		ms = _ms;
		this->func = _func;
		this->resolveAllDeps();
	}

	size_t getDescriptionHash();

	std::string debug_print() {
		return "[FunctionNode] " + name + " " + source_file_path + " " + to_string(debug_line);
	}

	static bool classof(const GeneralNode *GN) {
		return GN->getKind() == NK_Function;
	}

	unordered_set<size_t> InternalFunctionWaitlist;
	unordered_set<string> ExternalFunctionWaitlist;
	unordered_set<string> ExternalGlobalVarWaitlist;

	std::string prototypeRep;

	Function *func;

	ModuleSummary *ms;

private:
	
    void resolveAllDeps();
};

FunctionNode *createFuncNode(Function *F, size_t hash_idx, 
						ModuleSummary *ms, DependencyGraph *dg);
size_t getFunctionNode(Function *F, ModuleSummary *ms, 
											DependencyGraph *dg);
size_t getFunctionHash(Function *F);

#endif