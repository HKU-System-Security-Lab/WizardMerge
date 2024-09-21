#include <llvm/Support/CommandLine.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/ADT/StringRef.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>
#include <set>
#include <iostream>
#include <fstream>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "Utils.h"
#include "ProjectRepresentation.h"
#include "DiffTextSplit.h"
#include "GraphShrinking.h"
#include "GlobalVars.h"
#include "GraphGeneration/ModuleSummary.h"
#include "GraphGeneration/FunctionNode.h"
#include "GraphGeneration/GlobalVariableNode.h"
#include "GraphGeneration/TypeNode.h"

using namespace llvm;

GlobalVars g_vars;

std::set<ModifiedFileInVariant *>modified_files;
DependencyGraph *dgraph_va = nullptr;
DependencyGraph *dgraph_vb = nullptr;
ShrinkedGraph *sgraph_va = nullptr;
ShrinkedGraph *sgraph_vb = nullptr;
std::unordered_map<GeneralNode *, std::unordered_set<DiffCodeBlock *> > GeneralNodeToDCBs_va;
std::unordered_map<GeneralNode *, std::unordered_set<DiffCodeBlock *> > GeneralNodeToDCBs_vb;

typedef struct Parameter_s{
    explicit Parameter_s(std::string ind, std::string outd, std::string _ctx_project_dir_va, 
            std::string _ctx_project_dir_vb, std::string project_va, std::string project_vb, 
                int _job_num, std::string _use_proto_rep)
        : input_dir(ind), output_dir(outd), ctx_project_dir_va(_ctx_project_dir_va), 
            ctx_project_dir_vb(_ctx_project_dir_vb), project_root_dir_va(project_va),
                project_root_dir_vb(project_vb), job_num(_job_num), use_proto_rep(_use_proto_rep) {}

    std::string input_dir;
    std::string ctx_project_dir_va;
    std::string ctx_project_dir_vb;
    std::string project_root_dir_va;
    std::string project_root_dir_vb;
    std::string output_dir;
    std::string use_proto_rep;
    int job_num;
} Parameter;

bool compare_by_filename(ModuleSummary *a, ModuleSummary *b) {
    return a->filePath < b->filePath;
} 

bool compare_by_string(GeneralNode *a, GeneralNode *b) {
    return a->name + "~" + a->source_file_path == b->name + "~" + b->source_file_path
            ? a->debug_line < b->debug_line :
            a->name + "~" + a->source_file_path < b->name + "~" + b->source_file_path;
}

Parameter parseArguments(int argc, char **argv) {
    static cl::OptionCategory cat("split-file Options");   

    static cl::opt<std::string> input("i", cl::desc("Input directory"),
                                  cl::cat(cat));

    static cl::opt<std::string> project_dir_va("pa", cl::desc("Git project root directory for VA"),
                                  cl::cat(cat));

    static cl::opt<std::string> project_dir_vb("pb", cl::desc("Git project root directory for VB"),
                                  cl::cat(cat));

    static cl::opt<std::string> ctx_project_dir_va("ca", cl::desc("Git project root directory for current VA work context"),
                                  cl::cat(cat));

    static cl::opt<std::string> ctx_project_dir_vb("cb", cl::desc("Git project root directory for current VA work context"),
                                  cl::cat(cat));

    static cl::opt<std::string> output("o", cl::desc("Output directory"), cl::cat(cat));

    static cl::opt<int> job_num("j", cl::desc("Parallel job number"), cl::cat(cat));

    static cl::opt<std::string> use_proto_rep("proto_rep", cl::desc("Whether to use prototype-representation"), cl::cat(cat));

    cl::ParseCommandLineOptions(argc, argv);

    return Parameter(input, output, ctx_project_dir_va, ctx_project_dir_vb, 
                        project_dir_va, project_dir_vb, job_num, use_proto_rep);
}

long getIRFilesFromDirectory(std::set<std::string> &S, std::string Dir) {
    DIR *dir;
    struct dirent *ent;

    dir = opendir(Dir.c_str());
    if (!dir) return -1;

    while ((ent = readdir(dir)) != NULL) {
        std::string filename = ent->d_name;
        if (filename == "." || filename == "..") continue;
        
        S.insert(filename);
    }

    return S.size();
}

void IRFilesScanner(std::string CurrentDir, int job) {
    DIR *dir;
    struct dirent *ent;

    std::set<std::string>VA_IRFiles;
    std::set<std::string>VB_IRFiles;

    std::string VAIRPath = CurrentDir+ "/IR/" + "VA";
    std::string VBIRPath = CurrentDir+ "/IR/" + "VB";

    getIRFilesFromDirectory(VA_IRFiles, VAIRPath);
    getIRFilesFromDirectory(VB_IRFiles, VBIRPath);
    g_vars.va_ir_num = VA_IRFiles.size();
    g_vars.vb_ir_num = VB_IRFiles.size();

    printf("[DEBUG] Variant A is in IRFilesScanner\n");
    dgraph_va = new DependencyGraph();
    dgraph_va->threadPoolInit(job);
    int  i = 0;
    for (auto * MFV : modified_files) {
        std::string source_file_path = MFV->getSourceFileName();
        std::string IR_file_path = source_file_path;

        replace(IR_file_path, "/", "@");
        replace(IR_file_path, ".", "$");

        if (!VA_IRFiles.count(IR_file_path)) {
            printf("[A DEBUG] %s not in IR file set!\n", MFV->getSourceFileName().c_str());
            continue;
        }

        // printf("[A DEBUG] %s is hanlding\n", source_file_path.c_str());

        dgraph_va->enqueueModuleSummary(VAIRPath + "/" + IR_file_path, i++);
    }
    printf("[DEBUG] Variant A is waiting for summary\n");
    dgraph_va->waitForSummary();

    g_vars.va_ir_num = 0;

    // Merge all node and metadata with the same hash
    for (auto *m : dgraph_va->module_summaries) {

        // Merge for TypeNode
        for (auto p : m->TypeNodeList) {
            size_t TN_idx = p.first;
            TypeNode *TN = p.second;

            // TypeNode exist
            if (dgraph_va->TypeNodeList.count(TN_idx)) {
                if (m->TypeNodeGraph.count(TN_idx))
                    dgraph_va->TypeNodeGraph[TN_idx].insert(
                            m->TypeNodeGraph[TN_idx].begin(), m->TypeNodeGraph[TN_idx].end());
                // TODO
                // delete the node in the module to save memory
            }

            // New TypeNode
            else {
                dgraph_va->TypeNodeList[TN_idx] = TN;
                dgraph_va->TypeNodeToHash[TN] = TN_idx;

                // If the new TN has graph, create as well.
                if (m->TypeNodeGraph.count(TN_idx))
                    dgraph_va->TypeNodeGraph[TN_idx] = m->TypeNodeGraph[TN_idx];
            }
        }

        // Merge for GlobalVarNode
        for (auto p : m->GlobalVarNodeList) {
            size_t GN_idx = p.first;
            GlobalVariableNode *GN = p.second;

            // GVNode exist
            if (dgraph_va->GlobalVarNodeList.count(GN_idx)) {
                if (m->GlobalVarNodeGraph.count(GN_idx))
                    dgraph_va->GlobalVarNodeGraph[GN_idx].insert(
                            m->GlobalVarNodeGraph[GN_idx].begin(), m->GlobalVarNodeGraph[GN_idx].end());
                // TODO
                // delete the node in the module to save memory
            }

            // New GVNode
            else {
                dgraph_va->GlobalVarNodeList[GN_idx] = GN;
                dgraph_va->GlobalVarNodeToHash[GN] = GN_idx;

                // If the new TN has graph, create as well.
                if (m->GlobalVarNodeGraph.count(GN_idx))
                    dgraph_va->GlobalVarNodeGraph[GN_idx] = m->GlobalVarNodeGraph[GN_idx];
            }
        }

        // Merge for FuncNode
        for (auto p : m->FuncNodeList) {
            size_t FN_idx = p.first;
            FunctionNode *FN = p.second;

            // FuncNode exist
            if (dgraph_va->FuncNodeList.count(FN_idx)) {
                FunctionNode *standard_FN = dgraph_va->FuncNodeList[FN_idx];
                if (m->FunctionNodeGraph_Typedep_Proto.count(FN_idx))
                    dgraph_va->FunctionNodeGraph_Typedep_Proto[FN_idx].insert(
                            m->FunctionNodeGraph_Typedep_Proto[FN_idx].begin(), 
                            m->FunctionNodeGraph_Typedep_Proto[FN_idx].end());

                if (m->FunctionNodeGraph_Typedep_Body.count(FN_idx))
                    dgraph_va->FunctionNodeGraph_Typedep_Body[FN_idx].insert(
                            m->FunctionNodeGraph_Typedep_Body[FN_idx].begin(), 
                            m->FunctionNodeGraph_Typedep_Body[FN_idx].end());

                if (m->FunctionNodeGraph_GVdep.count(FN_idx))
                    dgraph_va->FunctionNodeGraph_GVdep[FN_idx].insert(
                            m->FunctionNodeGraph_GVdep[FN_idx].begin(), 
                            m->FunctionNodeGraph_GVdep[FN_idx].end());


                if (m->FunctionNodeGraph_Funcdep.count(FN_idx))
                    dgraph_va->FunctionNodeGraph_Funcdep[FN_idx].insert(
                            m->FunctionNodeGraph_Funcdep[FN_idx].begin(), 
                            m->FunctionNodeGraph_Funcdep[FN_idx].end());
                
                standard_FN->ExternalFunctionWaitlist.insert(
                    FN->ExternalFunctionWaitlist.begin(),
                    FN->ExternalFunctionWaitlist.end());

                standard_FN->ExternalGlobalVarWaitlist.insert(
                    FN->ExternalGlobalVarWaitlist.begin(),
                    FN->ExternalGlobalVarWaitlist.end());

                // TODO
                // delete the node in the module to save memory
            }

            // New FuncNode
            else {
                dgraph_va->FuncNodeList[FN_idx] = FN;
                dgraph_va->FuncNodeToHash[FN] = FN_idx;

                if (m->FunctionNodeGraph_Typedep_Proto.count(FN_idx))
                    dgraph_va->FunctionNodeGraph_Typedep_Proto[FN_idx] = m->FunctionNodeGraph_Typedep_Proto[FN_idx];
                if (m->FunctionNodeGraph_Typedep_Body.count(FN_idx))
                    dgraph_va->FunctionNodeGraph_Typedep_Body[FN_idx] = m->FunctionNodeGraph_Typedep_Body[FN_idx];
                if (m->FunctionNodeGraph_GVdep.count(FN_idx))
                    dgraph_va->FunctionNodeGraph_GVdep[FN_idx] = m->FunctionNodeGraph_GVdep[FN_idx];
                if (m->FunctionNodeGraph_Funcdep.count(FN_idx))
                    dgraph_va->FunctionNodeGraph_Funcdep[FN_idx] = m->FunctionNodeGraph_Funcdep[FN_idx];

            }
        }

        // Merge for external Nodes
        dgraph_va->externalNodeNameToGVHash.insert(
            m->externalNodeNameToGVHash.begin(),
            m->externalNodeNameToGVHash.end());

        dgraph_va->externalNodeNameToFuncHash.insert(
            m->externalNodeNameToFuncHash.begin(),
            m->externalNodeNameToFuncHash.end());

        // Merge for file metadata
        for (auto p : m->fileMapsToNodes)
            dgraph_va->fileMapsToNodes[p.first].insert(p.second.begin(), p.second.end());
    }

    // Additional analysis for external functions/global variables.
    for (auto *m : dgraph_va->module_summaries) {
        for (auto p : m->FuncNodeList) {
            size_t FN_idx = p.first;
            FunctionNode *FN = p.second;

            for (string external_name : FN->ExternalGlobalVarWaitlist) {
                if (dgraph_va->externalNodeNameToGVHash.count(external_name)) {
                    size_t GN_idx = dgraph_va->externalNodeNameToGVHash[external_name];
                    dgraph_va->FunctionNodeGraph_GVdep[FN_idx].insert(GN_idx);
                }
            }

            for (string external_name : FN->ExternalFunctionWaitlist) {
                if (dgraph_va->externalNodeNameToFuncHash.count(external_name)) {
                    size_t callee_FN_idx = dgraph_va->externalNodeNameToFuncHash[external_name];
                    dgraph_va->FunctionNodeGraph_Funcdep[FN_idx].insert(callee_FN_idx);
                }
            }
        }
    }

    // std::vector<size_t> sorter;
    // std::vector<ModuleSummary *> module_sorter;
    // for (auto p : dgraph_va->FuncNodeList) {
    //     sorter.push_back(p.first);
    // }
    // std::sort(sorter.begin(), sorter.end());
    // for (auto t_idx : sorter) {
    //     printf("From FunctionNode ");
    //     dgraph_va->FuncNodeList[t_idx]->debug_print();
    //     for (auto sub_tidx : dgraph_va->FunctionNodeGraph_Funcdep[t_idx]) {
    //         assert(dgraph_va->FuncNodeList.count(sub_tidx));
    //         printf("    To FunctionNode ");
    //         dgraph_va->FuncNodeList[sub_tidx]->debug_print();
    //     }
    //     printf("\n");
    // }
    // sorter.clear();

    // exit(0);

    // return;

    printf("[DEBUG] va has typenode %d, functionnode %d, globalvariablenode %d\n", dgraph_va->TypeNodeList.size(), 
            dgraph_va->FuncNodeList.size(), dgraph_va->GlobalVarNodeList.size());

    // Also need to summary for Variant B.
    g_vars.current_in_va = false;
    dgraph_vb = new DependencyGraph();
    dgraph_vb->threadPoolInit(job);
    printf("[DEBUG] Variant B is in IRFilesScanner\n");
    i = 0;

    for (auto * MFV : modified_files) {
        MFV = MFV->getMirror();
        std::string source_file_path = MFV->getSourceFileName();
        std::string IR_file_path = source_file_path;

        replace(IR_file_path, "/", "@");
        replace(IR_file_path, ".", "$");

        if (!VB_IRFiles.count(IR_file_path)) {
            printf("[B DEBUG] %s not in IR file set!\n", MFV->getSourceFileName().c_str());
            continue;
        }

        // printf("[B DEBUG] %s is hanlding\n", source_file_path.c_str());

        dgraph_vb->enqueueModuleSummary(VBIRPath + "/" + IR_file_path, i++);
    }
    printf("[DEBUG] Variant B is waiting for summary\n");
    dgraph_vb->waitForSummary();

        // Merge all node and metadata with the same hash
    for (auto *m : dgraph_vb->module_summaries) {

        // Merge for TypeNode
        for (auto p : m->TypeNodeList) {
            size_t TN_idx = p.first;
            TypeNode *TN = p.second;

            // TypeNode exist
            if (dgraph_vb->TypeNodeList.count(TN_idx)) {
                if (m->TypeNodeGraph.count(TN_idx))
                    dgraph_vb->TypeNodeGraph[TN_idx].insert(
                            m->TypeNodeGraph[TN_idx].begin(), m->TypeNodeGraph[TN_idx].end());
                // TODO
                // delete the node in the module to save memory
            }

            // New TypeNode
            else {
                dgraph_vb->TypeNodeList[TN_idx] = TN;
                dgraph_vb->TypeNodeToHash[TN] = TN_idx;

                // If the new TN has graph, create as well.
                if (m->TypeNodeGraph.count(TN_idx))
                    dgraph_vb->TypeNodeGraph[TN_idx] = m->TypeNodeGraph[TN_idx];
            }
        }

        // Merge for GlobalVarNode
        for (auto p : m->GlobalVarNodeList) {
            size_t GN_idx = p.first;
            GlobalVariableNode *GN = p.second;

            // GVNode exist
            if (dgraph_vb->GlobalVarNodeList.count(GN_idx)) {
                if (m->GlobalVarNodeGraph.count(GN_idx))
                    dgraph_vb->GlobalVarNodeGraph[GN_idx].insert(
                            m->GlobalVarNodeGraph[GN_idx].begin(), m->GlobalVarNodeGraph[GN_idx].end());
                // TODO
                // delete the node in the module to save memory
            }

            // New GVNode
            else {
                dgraph_vb->GlobalVarNodeList[GN_idx] = GN;
                dgraph_vb->GlobalVarNodeToHash[GN] = GN_idx;

                // If the new TN has graph, create as well.
                if (m->GlobalVarNodeGraph.count(GN_idx))
                    dgraph_vb->GlobalVarNodeGraph[GN_idx] = m->GlobalVarNodeGraph[GN_idx];
            }
        }

        // Merge for FuncNode
        for (auto p : m->FuncNodeList) {
            size_t FN_idx = p.first;
            FunctionNode *FN = p.second;

            // FuncNode exist
            if (dgraph_vb->FuncNodeList.count(FN_idx)) {
                FunctionNode *standard_FN = dgraph_vb->FuncNodeList[FN_idx];
                if (m->FunctionNodeGraph_Typedep_Proto.count(FN_idx))
                    dgraph_vb->FunctionNodeGraph_Typedep_Proto[FN_idx].insert(
                            m->FunctionNodeGraph_Typedep_Proto[FN_idx].begin(), 
                            m->FunctionNodeGraph_Typedep_Proto[FN_idx].end());

                if (m->FunctionNodeGraph_Typedep_Body.count(FN_idx))
                    dgraph_vb->FunctionNodeGraph_Typedep_Body[FN_idx].insert(
                            m->FunctionNodeGraph_Typedep_Body[FN_idx].begin(), 
                            m->FunctionNodeGraph_Typedep_Body[FN_idx].end());

                if (m->FunctionNodeGraph_GVdep.count(FN_idx))
                    dgraph_vb->FunctionNodeGraph_GVdep[FN_idx].insert(
                            m->FunctionNodeGraph_GVdep[FN_idx].begin(), 
                            m->FunctionNodeGraph_GVdep[FN_idx].end());


                if (m->FunctionNodeGraph_Funcdep.count(FN_idx))
                    dgraph_vb->FunctionNodeGraph_Funcdep[FN_idx].insert(
                            m->FunctionNodeGraph_Funcdep[FN_idx].begin(), 
                            m->FunctionNodeGraph_Funcdep[FN_idx].end());
                
                standard_FN->ExternalFunctionWaitlist.insert(
                    FN->ExternalFunctionWaitlist.begin(),
                    FN->ExternalFunctionWaitlist.end());

                standard_FN->ExternalGlobalVarWaitlist.insert(
                    FN->ExternalGlobalVarWaitlist.begin(),
                    FN->ExternalGlobalVarWaitlist.end());

                // TODO
                // delete the node in the module to save memory
            }

            // New FuncNode
            else {
                dgraph_vb->FuncNodeList[FN_idx] = FN;
                dgraph_vb->FuncNodeToHash[FN] = FN_idx;

                if (m->FunctionNodeGraph_Typedep_Proto.count(FN_idx))
                    dgraph_vb->FunctionNodeGraph_Typedep_Proto[FN_idx] = m->FunctionNodeGraph_Typedep_Proto[FN_idx];
                if (m->FunctionNodeGraph_Typedep_Body.count(FN_idx))
                    dgraph_vb->FunctionNodeGraph_Typedep_Body[FN_idx] = m->FunctionNodeGraph_Typedep_Body[FN_idx];
                if (m->FunctionNodeGraph_GVdep.count(FN_idx))
                    dgraph_vb->FunctionNodeGraph_GVdep[FN_idx] = m->FunctionNodeGraph_GVdep[FN_idx];
                if (m->FunctionNodeGraph_Funcdep.count(FN_idx))
                    dgraph_vb->FunctionNodeGraph_Funcdep[FN_idx] = m->FunctionNodeGraph_Funcdep[FN_idx];

            }
        }

        // Merge for external Nodes
        dgraph_vb->externalNodeNameToGVHash.insert(
            m->externalNodeNameToGVHash.begin(),
            m->externalNodeNameToGVHash.end());

        dgraph_vb->externalNodeNameToFuncHash.insert(
            m->externalNodeNameToFuncHash.begin(),
            m->externalNodeNameToFuncHash.end());

        // Merge for file metadata
        for (auto p : m->fileMapsToNodes)
            dgraph_vb->fileMapsToNodes[p.first].insert(p.second.begin(), p.second.end());
    }

    // Additional analysis for external functions/global variables.
    for (auto *m : dgraph_vb->module_summaries) {
        for (auto p : m->FuncNodeList) {
            size_t FN_idx = p.first;
            FunctionNode *FN = p.second;

            for (string external_name : FN->ExternalGlobalVarWaitlist) {
                if (dgraph_vb->externalNodeNameToGVHash.count(external_name)) {
                    size_t GN_idx = dgraph_vb->externalNodeNameToGVHash[external_name];
                    dgraph_vb->FunctionNodeGraph_GVdep[FN_idx].insert(GN_idx);
                }
            }

            for (string external_name : FN->ExternalFunctionWaitlist) {
                if (dgraph_vb->externalNodeNameToFuncHash.count(external_name)) {
                    size_t callee_FN_idx = dgraph_vb->externalNodeNameToFuncHash[external_name];
                    dgraph_vb->FunctionNodeGraph_Funcdep[FN_idx].insert(callee_FN_idx);
                }
            }
        }
    }

    printf("[DEBUG] vb has typenode %d, functionnode %d, globalvariablenode %d\n", dgraph_vb->TypeNodeList.size(), 
            dgraph_vb->FuncNodeList.size(), dgraph_vb->GlobalVarNodeList.size());
}

std::string parseFileName(std::string s) {
    return s.substr(s.find("@ ") + 2, s.find(", ") - s.find("@ ") - 2);
}

std::pair<int, int> parseLineNumbers(std::string s) {
    int first = stoi(s.substr(s.find(", ") + 2));
    s = s.substr(s.find(", ") + 2);
    int second = stoi(s.substr(s.find(", ") + 2));
    return std::make_pair(first, second);
}

// This is for parsing PCB from PreliminaryCodeBlock file.
void parsePreliminaryCodeBlock(
        std::string s, ModifiedFileInVariant *MFV_A, ModifiedFileInVariant *MFV_B) {
    DiffCodeBlock *DCB_A = NULL;
    DiffCodeBlock *DCB_B = NULL;
    DIFF_TYPE dt;
    if (s.find("+ ") != std::string::npos)
        dt = APPLYA;
    else if (s.find("- ") != std::string::npos)
        dt = APPLYB;
    else if (s.find("# ") != std::string::npos)
        dt = CONFLICT;
    
    int start_line = std::stoi(s.substr(2, s.find(", ") - 2));
    int length = std::stoi(s.substr(s.find(", ") + 2, s.find(", =") - s.find(", ") - 2));

    DCB_A = new DiffCodeBlock(true, MFV_A->getRelativeFileName(), MFV_A, start_line, length, dt);

    s = s.substr(s.find(" ,"));
    start_line = std::stoi(s.substr(2, s.find(", ") - 2));
    length = std::stoi(s.substr(s.find(", ") + 2));

    DCB_B = new DiffCodeBlock(false, MFV_B->getRelativeFileName(), MFV_B, start_line, length, dt);

    DCB_A->setMirror(DCB_B);
    DCB_B->setMirror(DCB_A);

    MFV_A->addDiffCodeBlock(DCB_A);
    MFV_B->addDiffCodeBlock(DCB_B);
}

// This is for parsing ranges from file.
void extractRangesByFiles(std::string path, ModifiedFileInVariant *MFV, std::string real_relative_path) {
    std::ifstream fin(path);
    std::string content;
    RANGE_TYPE rt;
    // std::cerr << "[extractRangesByFiles] path = " << path << " real_relative_path = " << real_relative_path 
    //                 << " MFV->source = " << MFV->getSourceFileName() << "\n";
    unsigned last_range_start = 0;

    while (getline(fin, content)) {
        if (content.find("f, ") != std::string::npos) {
            rt = FUNCTION;
        }
        else if (content.find("e, ") != std::string::npos || content.find("c, ") != std::string::npos) {
            rt = COMPOSITE_TYPE;
        }
        else if (content.find("g, ") != std::string::npos) {
            rt = GLOABL_VAR;
        }
        else if (content.find("b, ") != std::string::npos) {
            // rt = FUNCTION_BODY;
        }
        else if (content.find("t, ") != std::string::npos) {
            rt = ALIAS;
        }
        content = content.substr(3);
        unsigned start = std::stoi(content.substr(0, content.find(", ")));

        // Duplicate range, ignore
        if (start < last_range_start)
            break;
        
        last_range_start = start;

        content = content.substr(content.find(", ") + 2);
        unsigned end = std::stoi(content.substr(0, content.find(", ")));
        content = content.substr(content.find(", ") + 2);
        std::string name = content;
        // llvm::errs() << "name: " << name << " start: " << start << " end: " << end << " range_type: " << rt << "\n";
        RangeCodeBlock *RCB = new RangeCodeBlock(MFV->isFromVariantA(), 
                    real_relative_path, MFV, name, start, end - start + 1, rt);
        MFV->addRangeCodeBlock(RCB);
    }
    
}

void extractModifiedFiles(std::string GitRootDir_VA, std::string GitRootDir_VB, std::string CurrentDir) {
    std::string blocks_path = CurrentDir + "/PreliminaryCodeBlocks";
    std::ifstream fin(blocks_path); 

    std::string content;
    ModifiedFileInVariant *currentModifiedFileA = NULL;
    ModifiedFileInVariant *currentModifiedFileB = NULL;
    while (getline(fin, content)) {
        if (content.find("@ ") != std::string::npos) {
            std::string file_relate_path = parseFileName(content);
            std::string source_file_path_va = GitRootDir_VA + "/" + file_relate_path;
            std::string source_file_path_vb = GitRootDir_VB + "/" + file_relate_path;
            g_vars.diff_file_set.insert(file_relate_path);
            int MFA_lines = parseLineNumbers(content).first;
            int MFB_lines = parseLineNumbers(content).second;
            currentModifiedFileA = new ModifiedFileInVariant(file_relate_path, source_file_path_va, MFA_lines, true);
            currentModifiedFileB = new ModifiedFileInVariant(file_relate_path, source_file_path_vb, MFB_lines, false);
            currentModifiedFileA->setMirror(currentModifiedFileB);
            modified_files.insert(currentModifiedFileA);

            std::string range_VA = CurrentDir + "/Range/VA";
            std::string range_VB = CurrentDir + "/Range/VB";

            std::string real_file_path_va = currentModifiedFileA->getSourceFileName();
            std::string real_file_path_vb = currentModifiedFileB->getSourceFileName();
            std::string range_base_path_va = real_file_path_va;
            std::string range_base_path_vb = real_file_path_vb;
            replace(range_base_path_va, "/", "@");
            replace(range_base_path_va, ".", "$");
            replace(range_base_path_vb, "/", "@");
            replace(range_base_path_vb, ".", "$");

            range_VA = range_VA + "/" + range_base_path_va;
            range_VB = range_VB + "/" + range_base_path_vb;

            extractRangesByFiles(range_VA, currentModifiedFileA, currentModifiedFileA->getRelativeFileName());
            extractRangesByFiles(range_VB, currentModifiedFileB, currentModifiedFileB->getRelativeFileName());
        }
        else {
            parsePreliminaryCodeBlock(content, currentModifiedFileA, currentModifiedFileB);
        }
    } 
}

inline bool isGNodeMatchRange(GeneralNode *gn, RangeCodeBlock *rcb) {
    std::string r_name = rcb->getName();
    if (rcb->getStartLine() > gn->debug_line || rcb->getEndLine() < gn->debug_line)
        return false;
    if (r_name.find(gn->name) == std::string::npos && gn->name.find(r_name) == std::string::npos)
        return false;
    return (rcb->getRangeType() == COMPOSITE_TYPE && gn->getKind() == GeneralNode::NK_Type) ||
            (rcb->getRangeType() == ALIAS && gn->getKind() == GeneralNode::NK_Type) ||
            (rcb->getRangeType() == GLOABL_VAR && gn->getKind() == GeneralNode::NK_GlobalVar) ||
            (rcb->getRangeType() == FUNCTION && gn->getKind() == GeneralNode::NK_Function);
}

void getFastTagForModifiedNode(DependencyGraph *dg, bool is_va) {
    for (auto *MFV : modified_files) {
        if (!is_va) MFV = MFV->getMirror();
        std::string source_name = MFV->getRelativeFileName();
        if (dg->fileMapsToNodes.count(source_name)) {
            auto &node_set = dg->fileMapsToNodes[source_name];
            vector<GeneralNode *> sort_helper;
            for (auto idx : node_set) {
                if (dg->FuncNodeList.count(idx))
                    sort_helper.push_back(dg->FuncNodeList[idx]);
                else if (dg->GlobalVarNodeList.count(idx))
                    sort_helper.push_back(dg->GlobalVarNodeList[idx]);
                else if (dg->TypeNodeList.count(idx))
                    sort_helper.push_back(dg->TypeNodeList[idx]);
                else {
                    exit(-1);
                }
            }
            // vector<GeneralNode *> sort_helper = {};
            int total_nodes = dg->fileMapsToNodes[source_name].size();
            int tagged_nodes = 0;
            std::sort(sort_helper.begin(), sort_helper.end(), compare_by_debug_line);

            int i = 0;
            unsigned max_line_num = 0;

            printf("[DEBUG] mapping for %s start with total size: %d\n", source_name.c_str(), MFV->_range_code_blocks.size());

            // FIXME: What if a modified file has no range info?
            if (MFV->_range_code_blocks.size() == 0) {
                continue;
            }

            for (int i = 0, r_idx = 0; i < sort_helper.size(); i++) {
                int current_line = sort_helper[i]->debug_line;
                int grp_end_idx = i;
                for (int j = i + 1; j < sort_helper.size(); j++) {
                    if (sort_helper[j]->debug_line > current_line) {
                        grp_end_idx = j - 1;
                        break;
                    }
                    if (j == sort_helper.size() - 1)
                        grp_end_idx = j;
                }

                int k = r_idx;
                int last_range_start = 0;
                for (int j = i; j <= grp_end_idx; j++) {
                    auto *node = sort_helper[j];
                    printf("[DEBUG] Node name: %s, debug_line: %d, source_file_path: %s, kind: %d\n", 
                        node->name.c_str(), node->debug_line, node->source_file_path.c_str(), node->getKind());

                    for (k = r_idx; k < MFV->_range_code_blocks.size(); k++) {
                        std::string range_name = MFV->_range_code_blocks[k]->getName();
                        printf("    [DEBUG k: %d, r_idx: %d, total_size: %d] range name: %s, start_line: %d, end_line: %d, diff_type: %d\n", k, r_idx, MFV->_range_code_blocks.size(), MFV->_range_code_blocks[k]->getName().c_str(), 
                            MFV->_range_code_blocks[k]->getStartLine(), MFV->_range_code_blocks[k]->getEndLine(), MFV->_range_code_blocks[k]->getRangeType());

                        // Range is way far away from current line, stop.
                        if (MFV->_range_code_blocks[k]->getStartLine() > current_line)
                            break;

                        // Range has not catched up with the current line, we skip and wait.
                        if (MFV->_range_code_blocks[k]->getEndLine() < current_line) {
                            r_idx++;
                            continue;
                        }
                            
                        if (isGNodeMatchRange(node, MFV->_range_code_blocks[k])) {
                            tagged_nodes++;
                            node->start_line = MFV->_range_code_blocks[k]->getStartLine();
                            node->end_line = MFV->_range_code_blocks[k]->getEndLine();
                            printf("            [DEBUG] Node name: %s is tagged!\n", node->name.c_str());
                            MFV->_mapped_ranges.push_back(AbstractLine(
                                    node->start_line, node->end_line, node));
                            break;
                        }
                    }
                }
                i = grp_end_idx;
            }

            // for (auto *node : sort_helper) {
            //     printf("[DEBUG] Node name: %s, debug_line: %d, source_file_path: %s, kind: %d\n", 
            //         node->name.c_str(), node->debug_line, node->source_file_path.c_str(), node->getKind());
                
            //     bool is_end = false;

            //     // lower_range - upper_range - node
            //     while (MFV->_range_code_blocks[i]->getEndLine() < node->debug_line) {
            //         printf("    [DEBUG i: %d] range name: %s, start_line: %d, end_line: %d\n", i, MFV->_range_code_blocks[i]->getName().c_str(), 
            //                 MFV->_range_code_blocks[i]->getStartLine(), MFV->_range_code_blocks[i]->getEndLine());
            //         if (i + 1 == MFV->_range_code_blocks.size()) {
            //             is_end = true;
            //             break;
            //         }
            //         i++;
            //     }
            //     if (is_end) break;

            //     // lower_range - node - upper_range
            //     while (MFV->_range_code_blocks[i]->getStartLine() <= node->debug_line &&
            //             MFV->_range_code_blocks[i]->getEndLine() >= node->debug_line) {
            //         printf("    [DEBUG i : %d] range name: %s, start_line: %d, end_line: %d\n", i, MFV->_range_code_blocks[i]->getName().c_str(), 
            //                 MFV->_range_code_blocks[i]->getStartLine(), MFV->_range_code_blocks[i]->getEndLine());
            //         if (MFV->_range_code_blocks[i]->getName() == node->name) {
            //             tagged_nodes++;
            //             node->start_line = MFV->_range_code_blocks[i]->getStartLine();
            //             node->end_line = MFV->_range_code_blocks[i]->getEndLine();
            //             max_line_num = std::max(max_line_num, node->end_line);
            //             printf("[DEBUG] Node name: %s is tagged!\n", node->name.c_str());
            //             MFV->_mapped_ranges.push_back(AbstractLine(
            //                     node->start_line, node->end_line, node));
            //             if (i + 1 == MFV->_range_code_blocks.size()) is_end = true;
            //             else i++;
            //             break;
            //         }

            //         // Super judge if the name does not match, but the lines exactly match.
            //         else if (MFV->_range_code_blocks[i]->getStartLine() == node->debug_line &&
            //                     MFV->_range_code_blocks[i]->getStartLine() != MFV->_range_code_blocks[i]->getEndLine()) {
            //             llvm::errs() << "Super Judge for line-exactly-matching" << "\n";
            //             // tagged_nodes++;
            //             // node->start_line = MFV->_range_code_blocks[i]->getStartLine();
            //             // node->end_line = MFV->_range_code_blocks[i]->getEndLine();
            //             // max_line_num = std::max(max_line_num, node->end_line);
            //             // printf("[DEBUG] Node name: %s is tagged! (via SJ)\n", node->name.c_str());
            //             // MFV->_mapped_ranges.push_back(AbstractLine(
            //             //         node->start_line, node->end_line, node));
            //             // if (i + 1 == MFV->_range_code_blocks.size()) is_end = true;
            //             // else i++;
            //             // break;
            //         }
            //         if (i + 1 == MFV->_range_code_blocks.size()) {
            //             is_end = true;
            //             break;
            //         }
            //         i++;
            //     }
            //     if (is_end) break;

            //     // node - lower_range - upper_range
            //     if (MFV->_range_code_blocks[i]->getStartLine() > node->debug_line) {
            //         printf("    [DEBUG i : %d] range name: %s, start_line: %d, end_line: %d\n", i, MFV->_range_code_blocks[i]->getName().c_str(), 
            //                 MFV->_range_code_blocks[i]->getStartLine(), MFV->_range_code_blocks[i]->getEndLine());
            //         continue;
            //     }   
            // }

            printf("[DEBUG] In %s, %d nodes in total, %d are tagged.\n", source_name.c_str(), total_nodes, tagged_nodes);
        }
    }
}

void mapDiffBlockToNode(DependencyGraph *dg, bool is_va) {
    auto &gnode_dcb_set = is_va ? GeneralNodeToDCBs_va : GeneralNodeToDCBs_vb;
    for (auto *MFV : modified_files) {

        if (!is_va) MFV = MFV->getMirror();

        std::string source_name = MFV->getRelativeFileName();
        if (dg->fileMapsToNodes.count(source_name)) {
            DCB_NodeMapping(MFV->_mapped_ranges, MFV->_diff_code_blocks, 
                                    gnode_dcb_set, MFV->getLineNumber());
        }
        else {
            // TODO
            // Handle unknown files.
        }
    }
}

int main(int argc, char **argv) {
    setbuf(stdout, NULL);
    setbuf(stderr, NULL);
    Parameter P = parseArguments(argc, argv);
    g_vars.ctx_project_root_path_va = P.ctx_project_dir_va;
    g_vars.ctx_project_root_path_vb = P.ctx_project_dir_vb;
    g_vars.project_root_path_VA = P.project_root_dir_va;
    g_vars.project_root_path_VB = P.project_root_dir_vb;
    g_vars.current_in_va = true;
    g_vars.use_prototype_rep = P.use_proto_rep == "y" ? true : false;
    std::cerr << g_vars.use_prototype_rep << "\n";
    extractModifiedFiles(P.project_root_dir_va, P.project_root_dir_vb, P.input_dir);

    // Too slow, can be accelerate
    IRFilesScanner(P.input_dir, P.job_num);

    // Record for considered files
    for (auto nd : dgraph_va->fileMapsToNodes) {
        std::string output = "VA files: ";
        output += nd.first;
        printf("%s\n", output.c_str());
    } 

    for (auto nd : dgraph_vb->fileMapsToNodes) {
        std::string output = "VB files: ";
        output += nd.first;
        printf("%s\n", output.c_str());
    } 

    printf("[DEBUG] Variant A is in getFastTagForModifiedNode\n");
    getFastTagForModifiedNode(dgraph_va, true);
    printf("[DEBUG] Variant B is in getFastTagForModifiedNode\n");
    getFastTagForModifiedNode(dgraph_vb, false);
    
    printf("[DEBUG] Variant A is in mapDiffBlockToNode\n");
    mapDiffBlockToNode(dgraph_va, true);
    printf("[DEBUG] Variant B is in mapDiffBlockToNode\n");
    mapDiffBlockToNode(dgraph_vb, false);

    sgraph_va = new ShrinkedGraph();
    sgraph_vb = new ShrinkedGraph();

    printf("[DEBUG] Variant A is in buildShrinkedGraph\n");
    sgraph_va->buildShrinkedGraph(dgraph_va, GeneralNodeToDCBs_va);

    printf("[DEBUG] Variant B is in buildShrinkedGraph\n");
    sgraph_vb->buildShrinkedGraph(dgraph_vb, GeneralNodeToDCBs_vb);

    // std::sort(sgraph_va->debug_outputs.begin(), sgraph_va->debug_outputs.end());
    // for (auto s : sgraph_va->debug_outputs)
    //     std::cout << s << "\n";

    // exit(0);

    printf("[DEBUG] nodeMatching\n");
    sgraph_va->nodeMatching(sgraph_vb);

    printf("[DEBUG] violatedAppliedCBDetect\n");
    sgraph_va->violatedAppliedCBDetect(sgraph_vb);

    printf("[DEBUG] graphMerging\n");
    sgraph_va->graphMerging(sgraph_vb);

    printf("[DEBUG] emitSuggestions\n");
    sgraph_va->emitSuggestions(P.output_dir);

    // std::sort(sgraph_va->debug_outputs.begin(), sgraph_va->debug_outputs.end());
    // for (auto s : sgraph_va->debug_outputs)
    //     std::cout << s << "\n";

}
