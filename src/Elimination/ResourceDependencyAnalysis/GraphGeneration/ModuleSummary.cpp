#include "ModuleSummary.h"
#include "GlobalVariableNode.h"
#include "FunctionNode.h"
#include "Utils.h"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugInfo.h>
#include <mutex>
#include <sys/time.h>

int times = 0;

// unordered_set<std::string> ModuleSummary::included_files;
ThreadPool *summary_pool;

void ModuleSummary::resolveAll() {
	// Resolve all types in this module

	unordered_set<std::string> current_round_included_files;

	// errs() << " ===================================== " << filePath << " " << times << " ===================================== \n";
	struct timeval start, overall_start;
	struct timeval end, overall_end;
	total_FunctionNodeTime = total_TypeNodeTime = 0;
	
	IgnoredTypeNode.clear();
	TypeNodeCache.clear();
	DebugInfoFinder Finder;
	Finder.processModule(*M);
	gettimeofday(&overall_start, NULL);
	for (auto *Ty : Finder.types()) {

		unordered_set<size_t> results;
		bool is_valuable = getTypeNode(Ty, results, this, dg);
			
		// else {
			// for (auto *currentTN : results) {
			// 	// errs() << "[Current] TypeNode - > " << currentTN << " " << currentTN->TyPtr->getName() << "\n";
			// 	// errs() << "-- ";
			// 	// currentTN->TyPtr->dump();
			// 	for (auto *nextTN : TypeNode::TypeNodeGraph[currentTN]) {
			// 		// errs() << "    - [Parent] TypeNode -> "  << nextTN << " "  << nextTN->TyPtr->getName() 
			// 		// 			<< " " << nextTN->TyPtr->getFilename() << " " << nextTN->TyPtr->getLine() << "\n";
			// 		// errs() << "    -- ";
			// 		// nextTN->TyPtr->dump();
			// 	}
			// }	
				
		// }
		// errs() << "\n";
	}
	gettimeofday(&overall_end, NULL);
	total_TypeNodeTime = overall_end.tv_sec - overall_start.tv_sec;
	// printf("[types summary] time : %d secs\n", end.tv_sec - start.tv_sec);

	// errs() << "[Summary in Module] " << TypeNode::TypeNodeList.size() << " TypeNodes, " << TypeNode::IgnoredTypeNode.size() << " are ignored.\n";

	// Resolve all global vars in this module
	for (auto &gv : M->globals()) {

		// // Avoid duplicately solving types from the same file such as header files.
		// if (!Ty->getFilename().empty()) {
		// 	std::string full_path = getFullPath(Ty->getFilename().str(), Ty->getDirectory().str());
		// 	if (included_files.count(full_path)) continue;
		// 	current_round_included_files.insert(full_path);
		// }
		if (!gv.hasName()) continue;
		std::string name = "";
		if (gv.getGlobalIdentifier().length() == 0) name = gv.getName().str();
		name = gv.getGlobalIdentifier();

		// errs() << "\n[Processing] GlobalVar -> " << &gv << " " << name << "\n";
		if (!gv.isDeclaration()) {
			size_t gvN_idx = getGlobalVarNode(&gv, this, dg);
			if (!gvN_idx) {
				// errs() << "[UNKOWN] Globalvar " << name << " in " << filePath << " not handled!\n";
			}
				
			else {
				// errs() << "[Current] GlobalVarNode -> " << gvN << " " << gvN->DIgV->getDisplayName() << "\n";
				if (gv.hasExternalLinkage()) {
					GlobalVariableNode *gvN = GlobalVarNodeList[gvN_idx];
					assert(!externalNodeNameToGVHash.count(name) && "External GVNode exists!");
					externalNodeNameToGVHash[name] = gvN_idx;
					// newNodeToExternalName[gvN_idx] = name;
					// need unlock
				}
				// for (auto *nextTN : GlobalVariableNode::GlobalVarNodeGraph[gvN]) {
				// 	// errs() << "    - [Parent] TypeNode -> "  << nextTN << " "  << nextTN->TyPtr->getName() 
				// 	// 			<< " " << nextTN->TyPtr->getFilename() << " " << nextTN->TyPtr->getLine() << "\n";
				// 	// errs() << "    -- ";
				// 	// nextTN->TyPtr->dump();
				// }
			}
		}
		// errs() << "\n";
	}
	// printf("[gv summary] time : %d secs\n", end.tv_sec - start.tv_sec);

	// errs() << "[Summary in Module] " << GlobalVariableNode::GlobalVarNodeList.size() << " GVNodes.\n";

	// Resolve all functions in this module
	gettimeofday(&overall_start, NULL);
	for (Function &F : M->functions()) {
		std::string name = "";
		if (F.getGlobalIdentifier().length() == 0) name = F.getName().str();
		name = F.getGlobalIdentifier();

		// errs() << "\n[" << thread_idx << "] Processing Function -> " << name << "\n";
		if (!F.isDeclaration())  {
			// gettimeofday(&start, NULL);
			// errs() << "\n[" << thread_idx << "] geting Function node -> " << name << "\n";
			size_t FN_idx = getFunctionNode(&F, this, dg);
			// errs() << "\n[" << thread_idx << "] Function node hash -> " << FN_idx << "\n";
			// gettimeofday(&end, NULL);
			// FunctionNode *FN = NULL;
			if (!FN_idx) {}
				// errs() << "[UNKOWN] Function " << F.getName() << " in " << filePath << " not handled!\n";
			else {
				FunctionNode *FN = FuncNodeList[FN_idx];
				FunctionNodeAnalysisTimeRecord[FN_idx] = (end.tv_usec - start.tv_usec) / 1000.0;
				if (F.hasExternalLinkage() && name != "main" && 
							name != "LLVMFuzzerTestOneInput") {
					// need lock
					// printf("[Debug] detect external function name: %s\n		[Debug] file: %s\n", name.c_str(), F.getSubprogram()->getFilename().str().c_str());
					assert(!externalNodeNameToFuncHash.count(name) && "External FuncNode exists!");
					externalNodeNameToFuncHash[name] = FN_idx;
					// newNodeToExternalName[dyn_cast<GeneralNode>(FN)] = name;
					// need unlock
				}
				// errs() << "[Function idx] idx = " << getFunctionHash(&F) << " name = " << name << "\n";
				// for (auto *nextTN: FunctionNode::FunctionNodeGraph_Typedep_Proto[FN]) {
				// 	// errs() << "    - [Parent] Prototype depended Type -> " << nextTN << " " << nextTN->TyPtr->getName() << " " << nextTN->TyPtr->getFilename() << " " << nextTN->TyPtr->getLine() << "\n";
				// 	// errs() << "    -- ";
				// 	// nextTN->TyPtr->dump();
				// }

				// for (auto *nextTN: FunctionNode::FunctionNodeGraph_Typedep_Body[FN]) {
				// 	// errs() << "    - [Parent] Body depended Type -> " << nextTN << " " << nextTN->TyPtr->getName() << " " << nextTN->TyPtr->getFilename() << " " << nextTN->TyPtr->getLine() << "\n";
				// 	// errs() << "    -- ";
				// 	// nextTN->TyPtr->dump();
				// }

				// for (auto *gvN: FunctionNode::FunctionNodeGraph_GVdep[FN]) {
				// 	std::string nextName = "";
				// 	if (gvN->DIgV->getLinkageName().empty()) nextName = gvN->DIgV->getDisplayName().str();
				// 	else nextName = gvN->DIgV->getLinkageName().str();
				// 	// errs() << "    - [Parent] GlobalVar -> " << gvN << " " << nextName << "\n";
				// 	// errs() << "    -- ";
				// 	// gvN->DIgV->dump();
				// }

				// for (std::string pending_gv_name : FN->ExternalGlobalVarWaitlist) {
				// 	// errs() << "	   - [Parent external] GlobalVar -> " << pending_gv_name << "\n";
				// }

				// for (auto *nextFN : FunctionNode::FunctionNodeGraph_Funcdep[FN]) {
				// 	std::string nextName = "";
				// 	if (nextFN->func->getGlobalIdentifier().length() == 0) nextName = nextFN->func->getName().str();
				// 	else nextName = nextFN->func->getGlobalIdentifier();
				// 	errs() << "    - [Parent] Function -> " << nextFN << " " << nextName << "\n";
				// 	errs() << "    -- ";
				// 	nextFN->func->getSubprogram()->dump();
				// }

				// for (size_t pending_func_name : FN->InternalFunctionWaitlist) {
				// 	// errs() << "	   - [Parent Internal] Function -> " << pending_func_name << "\n";
				// }

				// for (std::string pending_func_name : FN->ExternalFunctionWaitlist) {
				// 	// errs() << "	   - [Parent external] Function -> " << pending_func_name << "\n";
				// }

				
			}
		}
		else {
			// errs() << "[DEBUG] delclaration detected! Ignore...\n";
		}	
		// errs() << "\n";
	}
	gettimeofday(&overall_end, NULL);
	total_FunctionNodeTime = overall_end.tv_sec - overall_start.tv_sec;
	// printf("[function summary] time : %d secs\n", end.tv_sec - start.tv_sec);

	// Additional linking for internal used function relation.
	gettimeofday(&start, NULL);
	for (Function &F : M->functions()) {
		if (!F.isDeclaration())  {
			size_t FN_idx = getFunctionNode(&F, this, dg);
			if (!FN_idx) continue;
			else {
				FunctionNode *FN = FuncNodeList[FN_idx];
				for (size_t func_node_idx : FN->InternalFunctionWaitlist) {
					// if (!FuncNodeList.count(func_node_idx)) {
					// 	dg->print_mtx.lock();
					// 	printf("[%d] wrong function ===> %s\n", thread_idx, FN->name.c_str());
					// 	printf("[%d] not found... ===> %llu\n", thread_idx, func_node_idx);
					// 	dg->print_mtx.unlock();
					// }
					assert(FuncNodeList.count(func_node_idx) 
									&& "Internally used function not found!");
					
					FunctionNodeGraph_Funcdep[FN_idx].insert(func_node_idx);
				}
			}
		}
	}
	gettimeofday(&end, NULL);
	// printf("[function linking summary] time : %d secs\n", end.tv_sec - start.tv_sec);

	// for (Function &F : M->functions()) {
	// 	std::string name = "";
	// 	if (F.getGlobalIdentifier().length() == 0) name = F.getName().str();
	// 	name = F.getGlobalIdentifier();

	// 	errs() << "\n[Mapping Internal Callee] Function -> " << name << "\n";
	// 	if (!F.isDeclaration())  {
	// 		FunctionNode *FN = getFunctionNode(&F);
	// 		if (FN) {
	// 			for (size_t pending_func_name : FN->InternalFunctionWaitlist) {
	// 				if (FunctionNode::FuncNodeList.count(pending_func_name)) {
	// 					FunctionNode::FunctionNodeGraph_Funcdep[FN].insert(
	// 						FunctionNode::FuncNodeList[pending_func_name]
	// 					);
	// 				}
	// 				else {
	// 					errs() << "[WARNING] " << pending_func_name << " not found a matching function!\n";
	// 				}
	// 			}

	// 			for (auto *nextFN : FunctionNode::FunctionNodeGraph_Funcdep[FN]) {
	// 				std::string nextName = "";
	// 				if (nextFN->func->getGlobalIdentifier().length() == 0) nextName = nextFN->func->getName().str();
	// 				else nextName = nextFN->func->getGlobalIdentifier();
	// 				errs() << "    - [Parent] Function -> " << nextFN << " " << nextName << "\n";
	// 				errs() << "    -- ";
	// 				nextFN->func->getSubprogram()->dump();
	// 			}
	// 		}
	// 	}
	// }

	// errs() << "[Summary in Module] " << FunctionNode::FuncNodeList.size() << " Functions.\n";

	// included_files.insert(current_round_included_files.begin(),
	// 				current_round_included_files.end());

	// if (times == 20) exit(0);
	times++;
	dg->print_mtx.lock();
	printf("[Processing] %d / %d --- %s\n", thread_idx, g_vars.va_ir_num == 0 ? g_vars.vb_ir_num : g_vars.va_ir_num, filePath.c_str());
	dg->print_mtx.unlock();
}

ModuleSummary *createModuleSummary(std::string file_path, DependencyGraph *dg, int idx) {
	return new ModuleSummary(file_path, dg, idx);
}