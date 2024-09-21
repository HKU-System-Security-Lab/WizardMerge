#ifndef __DEPENDENCYGRAPH_H
#define __DEPENDENCYGRAPH_H

#include "ThreadPool.h"

#include <mutex>

class GeneralNode;
class FunctionNode;
class TypeNode;
class GlobalVariableNode;
class ModuleSummary;
class DependencyGraph;

ModuleSummary *createModuleSummary(std::string file_path, DependencyGraph *dg, int idx);

class DependencyGraph {

public:
    DependencyGraph() {}

    // General Use
    std::mutex print_mtx;
    std::unordered_map<string, std::unordered_set<size_t> > fileMapsToNodes;
    std::unordered_map<string, size_t> externalNodeNameToGVHash;
    std::unordered_map<string, size_t> externalNodeNameToFuncHash;

    // FunctionNode
    std::unordered_map<size_t, std::unordered_set<size_t> > FunctionNodeGraph_Typedep_Proto;
    std::unordered_map<size_t, std::unordered_set<size_t> > FunctionNodeGraph_Typedep_Body; 
    std::unordered_map<size_t, std::unordered_set<size_t> > FunctionNodeGraph_GVdep; 
    std::unordered_map<size_t, std::unordered_set<size_t> > FunctionNodeGraph_Funcdep; 
	std::unordered_map<size_t, FunctionNode *> FuncNodeList;
    std::unordered_map<FunctionNode *, size_t> FuncNodeToHash;

    // TypeNode
    std::unordered_map<size_t, std::unordered_set<size_t> > TypeNodeGraph; 
	std::unordered_map<size_t, TypeNode *> TypeNodeList;
    std::unordered_map<TypeNode *, size_t> TypeNodeToHash;

    // GlobalvarNode
    std::unordered_map<size_t, std::unordered_set<size_t> > GlobalVarNodeGraph;
    std::unordered_map<size_t, GlobalVariableNode *> GlobalVarNodeList;
    std::unordered_map<GlobalVariableNode *, size_t> GlobalVarNodeToHash;

    void enqueueModuleSummary(std::string _filePath, int idx) {
        tasks_results.push_back(summary_pool->enqueue(0, createModuleSummary, _filePath, this, idx));
        // module_summaries.push_back(createModuleSummary(_filePath, this, idx));
    }

    void waitForSummary() {
        // return;
        for (auto && result : tasks_results) {
            module_summaries.push_back(result.get());
        }

        delete summary_pool;
        summary_pool = nullptr;
    }

    void threadPoolInit(int size = 8) {
        // return;
        summary_pool = new ThreadPool(size);
    }

    std::vector<ModuleSummary *> module_summaries;

private:
    std::vector<std::future<ModuleSummary *> > tasks_results;
    ThreadPool *summary_pool;
};

#endif