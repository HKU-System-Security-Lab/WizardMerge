#ifndef MODULESUMMARY_H
#define MODULESUMMARY_H

#include <unordered_set>

#include "TypeNode.h"
#include "ThreadPool.h"

class TypeNode;
class DependencyGraph;

class ModuleSummary {
public:
    ModuleSummary(string _filePath, DependencyGraph *_dg, int idx) {
        M = parseIRFile(_filePath, smd, ctx);
        dg = _dg;
        thread_idx = idx;
        if (!M) {
            errs() << "[WARNING] Creating " << _filePath << " failed!\n";
        }

        else {
            filePath = _filePath;
            resolveAll();
        }
    }

    // static unordered_set<std::string> included_files;
    unordered_map<DIType *, unordered_set<size_t> > TypeNodeCache;
	unordered_set<DIType *> IgnoredTypeNode;

    // General Use
    std::unordered_map<size_t, FunctionNode *> newFuncNodes;
    std::unordered_map<size_t, TypeNode *> newTypeNodes;
    std::unordered_map<size_t, GlobalVariableNode *> newGVNodes;
    std::unordered_map<GeneralNode *, string> newNodeToFile;

    std::unordered_map<string, std::unordered_set<size_t> > fileMapsToNodes;
    std::unordered_map<string, size_t> externalNodeNameToGVHash;
    std::unordered_map<string, size_t> externalNodeNameToFuncHash;

    // FunctionNode
	// std::unordered_map<FunctionNode *, std::unordered_set<TypeNode *> > FunctionNodeGraph_Typedep_Proto;
    std::unordered_map<size_t, std::unordered_set<size_t> > FunctionNodeGraph_Typedep_Proto;
    // std::unordered_map<FunctionNode *, std::unordered_set<size_t> > FunctionNodeGraph_Typedephash_Proto;  
	// std::unordered_map<FunctionNode *, std::unordered_set<TypeNode *> > FunctionNodeGraph_Typedep_Body; 
    std::unordered_map<size_t, std::unordered_set<size_t> > FunctionNodeGraph_Typedep_Body; 
    // std::unordered_map<FunctionNode *, std::unordered_set<size_t> > FunctionNodeGraph_Typedephash_Body;
	// std::unordered_map<FunctionNode *, std::unordered_set<GlobalVariableNode *> > FunctionNodeGraph_GVdep; 
    std::unordered_map<size_t, std::unordered_set<size_t> > FunctionNodeGraph_GVdep; 
    // std::unordered_map<FunctionNode *, std::unordered_set<size_t> > FunctionNodeGraphhash_GVdep; 
    std::unordered_map<size_t, std::unordered_set<size_t> > FunctionNodeGraph_Funcdep; 
	std::unordered_map<size_t, FunctionNode *> FuncNodeList;
    std::unordered_map<FunctionNode *, size_t> FuncNodeToHash;
	// std::unordered_map<FunctionNode *, std::unordered_set<FunctionNode *> > FunctionNodeGraph_Funcdep; 
    // std::unordered_map<FunctionNode *, std::unordered_set<size_t> > FunctionNodeGraphhash_Funcdep; 

    // TypeNode
	// std::unordered_map<TypeNode *, std::unordered_set<TypeNode *> > TypeNodeGraph; 
    // std::unordered_map<TypeNode *, std::unordered_set<size_t> > TypeNodeGraphhash; 
    std::unordered_map<size_t, std::unordered_set<size_t> > TypeNodeGraph; 
	std::unordered_map<size_t, TypeNode *> TypeNodeList;
    std::unordered_map<TypeNode *, size_t> TypeNodeToHash;

    // GlobalvarNode
    std::unordered_map<size_t, std::unordered_set<size_t> > GlobalVarNodeGraph;
    std::unordered_map<size_t, GlobalVariableNode *> GlobalVarNodeList;
    std::unordered_map<GlobalVariableNode *, size_t> GlobalVarNodeToHash;
    // std::unordered_map<GlobalVariableNode *, std::unordered_set<TypeNode *> > GlobalVarNodeGraph;
    // std::unordered_map<GlobalVariableNode *, std::unordered_set<size_t> > GlobalVarNodeGraphhash;

    std::unordered_map<size_t, double> FunctionNodeAnalysisTimeRecord;
    std::unordered_map<size_t, double> FunctionNodeAnalysisTimeRecord_type;
    int total_FunctionNodeTime;
    int total_TypeNodeTime;

    std::unique_ptr<Module> M;
    llvm::LLVMContext ctx;
    llvm::SMDiagnostic smd;

	string filePath;

    int thread_idx;

    DependencyGraph *dg;

private:
	void resolveAll();
};

ModuleSummary *createModuleSummary(std::string file_path, DependencyGraph *dg, int idx);

#endif