#include "FunctionNode.h"
#include "Utils.h"

#include <llvm/DebugInfo/DWARF/DWARFTypePrinter.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugInfo.h>
#include <sys/time.h>

int call_depth = 0;
int max_call_depth = 0;

void DebugInfoHelper::processInstruction(Instruction &Inst) {
	if (auto *DVI = dyn_cast<DbgVariableIntrinsic>(&Inst)) {
		auto *N = dyn_cast<MDNode>(DVI->getVariable());
		if (!N)
			return;

		auto *DV = dyn_cast<DILocalVariable>(N);
		if (!DV)
			return;

		if (!NodesSeen.insert(DV).second)
			return;
		processScope(DV->getScope());
		processType(DV->getType());
	}

  	if (auto DbgLoc = Inst.getDebugLoc())
    	processLocation(DbgLoc.get());
}

void DebugInfoHelper::processLocation(DILocation *Loc) {
	if (!Loc)
		return;
	processScope(Loc->getScope());
	processLocation(Loc->getInlinedAt());
}

void DebugInfoHelper::processScope(DIScope *Scope) {
	if (!Scope)
		return;
	if (auto *Ty = dyn_cast<DIType>(Scope)) {
		processType(Ty);
		return;
	}
}

void DebugInfoHelper::processType(DIType *DT){
	if (!addType(DT))
    	return;
  	processScope(DT->getScope());
}

bool DebugInfoHelper::addType(DIType *DT) {
  if (!DT)
    return false;

  if (!NodesSeen.insert(DT).second)
    return false;

  types.push_back(const_cast<DIType *>(DT));
  return true;
}

void recursivelyParseTypeName(DIType *Ty, std::string &cur_rep, std::string debug_clue) {
	// errs() << "			subTy = " << Ty << " ";
	// Ty->dump();
	if (auto *t = dyn_cast<DIBasicType>(Ty)) {
		cur_rep += "[" + t->getName().str() + "]";
		return;
	}
	if (auto *t = dyn_cast<DIStringType>(Ty)) {
		// Should never reach here for C/C++ program
		llvm_unreachable("DIStringType not considered!");
	}
	if (auto *t = dyn_cast<DISubroutineType>(Ty)) {
		cur_rep += "{";
		int valid_subTy_num = 0;
		for (DIType *subTy : t->getTypeArray()) {
			if (subTy) {
				valid_subTy_num++;
				recursivelyParseTypeName(subTy, cur_rep, debug_clue);
			}
		}
		cur_rep += "}=" + std::to_string(valid_subTy_num);
		return;
	}
	if (auto *t = dyn_cast<DICompositeType>(Ty)) {
		if (!t->getIdentifier().empty())
			cur_rep += "[" + t->getIdentifier().str() + "]";
		else if (!t->getName().empty())
			cur_rep += "[" + t->getName().str() + "]";
		else if (t->getTag() == llvm::dwarf::DW_TAG_array_type) {
			cur_rep += "[...";
			recursivelyParseTypeName(t->getBaseType(), cur_rep, debug_clue);
			cur_rep += "]";
		}
		else {
			t->print(llvm::errs(), nullptr);
			errs() << debug_clue << "\n";
			llvm_unreachable("Cannot found name of DICompositeType");
		}
		return;
	}
	if (auto *t = dyn_cast<DIDerivedType>(Ty)) {
		if (!t->getName().empty())
			cur_rep += "[" + t->getName().str() + "]";

		// If a ptr is passed
		else if (t->getTag() == llvm::dwarf::DW_TAG_pointer_type) {
			cur_rep += "[*";
			if (t->getBaseType())
				recursivelyParseTypeName(t->getBaseType(), cur_rep, debug_clue);
			else 
				cur_rep += "[void]";
			cur_rep += "]";
		}
	

		// If the current type is a const.
		else if (t->getTag() == llvm::dwarf::DW_TAG_const_type) {
			cur_rep += "[const";
			if (t->getBaseType())
				recursivelyParseTypeName(t->getBaseType(), cur_rep, debug_clue);
			else 
				cur_rep += "[void]";
			cur_rep += "]";
		}

		// If a volatile key word is set
		else if (t->getTag() == llvm::dwarf::DW_TAG_volatile_type) {
			cur_rep += "[volatile";
			if (t->getBaseType())
				recursivelyParseTypeName(t->getBaseType(), cur_rep, debug_clue);
			else 
				cur_rep += "[void]";
			cur_rep += "]";
		}

		// Do I miss something?
		else {
			t->print(llvm::errs(), nullptr);
			errs() << debug_clue << "\n";
			llvm_unreachable("Cannot found type of DIDerivedType");
		}
		return;
	}
}

void FunctionNode::resolveAllDeps() {
	struct timeval start;
	struct timeval end;
	DISubprogram *Sp = func->getSubprogram();
	DISubroutineType *sTy = Sp->getType();

	if (g_vars.use_prototype_rep) {
		recursivelyParseTypeName(sTy, prototypeRep, name);
		prototypeRep = name + prototypeRep;
	}

	mangled_name = Sp->getLinkageName().str();
	
    ms->fileMapsToNodes[source_file_path].insert(hash_val);
	ms->newNodeToFile[dyn_cast<GeneralNode>(this)] = source_file_path;

	gettimeofday(&start, NULL);
	for (DIType *subTy : sTy->getTypeArray()) {
		if (subTy) {
			// errs() << "[DEBUG FunctionNode::resolveAllDeps] prototype subTy = " << subTy << "\n";
			// subTy->dump();
			unordered_set<size_t> results;
			bool is_valuable = getTypeNode(subTy, results, ms, dg, 1);
			// bool is_valuable = false;
			if (is_valuable) {
				for (auto nextN_idx : results) {
					ms->FunctionNodeGraph_Typedep_Proto[hash_val].insert(nextN_idx);
					// ms->FunctionNodeGraph_Typedephash_Proto[hash_val].insert(getTypeHash(nextN->TyPtr));
				}		
			}
		}
	}
	gettimeofday(&end, NULL);
	// printf("[function's prototype summary] time : %d secs\n", end.tv_sec - start.tv_sec);

	// Then, we resolve body types.
	// processSubprogram is private in llvm 12.0.0
	// so we have to use processInstr to parse one by one.
	unordered_set<DIType *> handledTys;
	DebugInfoFinder Finder;

	gettimeofday(&start, NULL);
	// for (auto &BB : *func)
	// 	for (auto &I : BB)	
	// 		Finder.processInstruction(*func->getParent(), I);

	// DebugInfoHelper Helper(func, this->name);
	DebugInfoHelper Helper(func);


	// for (DIType *Ty : Finder.types()) {
	for (DIType *Ty : Helper.types) {
		if (ms->IgnoredTypeNode.count(Ty)) continue;
		if (handledTys.count(Ty)) {
			// errs() << "[DEBUG FunctionNode::resolveAllDeps] duplicate subTy = " << Ty << "\n";
			continue;
		}
		// errs() << "before emplacing size = " << handledTys.size() << "\n";
		handledTys.emplace(Ty);
		// errs() << "after emplacing size = " << handledTys.size() << "\n";
		
		// unordered_set<TypeNode *> results;

		assert(ms->TypeNodeCache.count(Ty));
		for (auto nextN_idx : ms->TypeNodeCache[Ty]) {
			ms->FunctionNodeGraph_Typedep_Body[hash_val].insert(nextN_idx);
			// if (this->name == "_ZN7mozilla17EventStateManager10WheelPrefs11GetIndexForEPKNS_16WidgetWheelEventE") {
			// 	errs() << "[DEBUG FunctionNode::resolveAllDeps] body subTy = " << ms->TypeNodeList[nextN_idx]->debug_print() << "\n";
			// }
				
			// ms->FunctionNodeGraph_Typedephash_Body[this].insert((size_t)nextN);
			// ms->FunctionNodeGraph_Typedephash_Body[this].insert(getTypeHash(nextN->TyPtr));
		}
		// if (this->name == "_ZN7mozilla17EventStateManager10WheelPrefs11GetIndexForEPKNS_16WidgetWheelEventE") {
		// 	errs() << "-----------------------------------" << "\n";
		// }

		// ms->FunctionNodeGraph_Typedep_Body[this].insert()

		// bool is_valuable = getTypeNode(Ty, results, ms, dg, 1);
		// bool is_valuable = false;
		// if (is_valuable) {
		// 	for (auto *nextN : results) {
		// 		ms->FunctionNodeGraph_Typedep_Body[this].insert(nextN);
		// 		ms->FunctionNodeGraph_Typedephash_Body[this].insert(getTypeHash(nextN->TyPtr));
		// 	}
		// }
		// else {
		// 	ms->FunctionNodeGraph_Typedep_Body[this].insert(nullptr);
		// }
	}
	gettimeofday(&end, NULL);
	ms->FunctionNodeAnalysisTimeRecord_type[hash_val] = (end.tv_usec - start.tv_usec) / 1000.0;
	// printf("[function's body types summary] time : %d secs\n", end.tv_sec - start.tv_sec);
	
	// Resolve all global variables used in the function
	gettimeofday(&start, NULL);
	for (BasicBlock &BB : *func) {
		for (Instruction &I : BB) {
			for (int i = 0; i < I.getNumOperands(); i++) {
                Value *V = I.getOperand(i);
                        
                if (GlobalVariable *GV = dyn_cast<GlobalVariable>(V)) {
					// The used global var is inside current translation unit.
					if (!GV->isDeclaration()) {
						size_t nextN_idx = getGlobalVarNode(GV, ms, dg);
						if (nextN_idx) {
							ms->FunctionNodeGraph_GVdep[hash_val].insert(nextN_idx);
							// ms->FunctionNodeGraphhash_GVdep[this].insert(getGlobalVarHash(nextN->DIgV));
						}
					}

					// Otherwise we need to store it's name for 
					// further inter-procedural search.
					else {
						string gvName;
						if (GV->getGlobalIdentifier().length() != 0)
							gvName = GV->getGlobalIdentifier();
						else gvName = GV->getName().str();
						ExternalGlobalVarWaitlist.insert(gvName);
					}
				}
			}
		}
	}
	gettimeofday(&end, NULL);
	// printf("[function's gv summary] time : %d secs\n", end.tv_sec - start.tv_sec);

	// Resolve all called functions
	gettimeofday(&start, NULL);
	for (BasicBlock &BB : *func) {
		for (Instruction &I : BB) {
			Instruction *inst = &I;

			if (CallInst *callInst = dyn_cast<CallInst>(inst)) {
				Function *calledFunc = callInst->getCalledFunction();

				// Avoid recursively invocation.
				if (calledFunc == func) continue;
				if (calledFunc && !calledFunc->isIntrinsic()) {
					// No matter whether the callee is inside current translation unit
					// or not, we only care about its name to avoid digging into the
					// function call tree.
					// string calleeName;
					// if (calledFunc->getGlobalIdentifier().length() != 0)
					// 	calleeName = calledFunc->getGlobalIdentifier();
					// else calleeName = calledFunc->getName().str();
					// FunctionWaitlist.insert(calleeName); 

					// Great! the called func is inside current translation unit.
					if (!calledFunc->isDeclaration()) {
						if (calledFunc->isIntrinsic() || !calledFunc->getSubprogram() ||
								isValidDebugInfo(calledFunc->getSubprogram()) != 1) {
							string calleeName;
							if (calledFunc->getGlobalIdentifier().length() != 0)
								calleeName = calledFunc->getGlobalIdentifier();
							else calleeName = calledFunc->getName().str();
							ExternalFunctionWaitlist.insert(calleeName);
						}
						else if (!calledFunc->getSubprogram()->getName().empty()){
							size_t next_node_idx = getFunctionHash(calledFunc);
							// dg->print_mtx.lock();
							// printf("[%d] caller function %p ---> %s\n", ms->thread_idx, func, this->name.c_str());
							// printf("[%d] callee function %p ---> %s\n", ms->thread_idx, calledFunc, calledFunc->getName().str().c_str());
							// printf("[%d] callee function 			%s, %d\n", ms->thread_idx, calledFunc->getSubprogram()->getFilename().str().c_str(), calledFunc->getSubprogram()->getLine());
							// calledFunc->getSubprogram()->print(llvm::errs(), nullptr);
							
							// printf("[%d] callee idx ---> %llu\n", ms->thread_idx, next_node_idx);
							// dg->print_mtx.unlock();
							// printf("build internal relation %lld call %lld\n", getFunctionHash(func), next_node_idx);
							InternalFunctionWaitlist.insert(next_node_idx);
						}
						// // call_depth++;
						// FunctionNode *nextN = getFunctionNode(calledFunc);
						// // call_depth--;
						// if (nextN) {
						// 	dg->FunctionNodeGraph_Funcdep[this].insert(nextN);
						// }
					}

					// Otherwise we only store it's name for further inter-procedural search
					else {
						string calleeName;
						if (calledFunc->getGlobalIdentifier().length() != 0)
							calleeName = calledFunc->getGlobalIdentifier();
						else calleeName = calledFunc->getName().str();
						ExternalFunctionWaitlist.insert(calleeName); 
					}
				}
			} else if (InvokeInst *invokeInst = dyn_cast<InvokeInst>(inst)) {
				Function *calledFunc = invokeInst->getCalledFunction();

				// Avoid recursively invocation.
				if (calledFunc == func) continue;
				if (calledFunc && !calledFunc->isIntrinsic()) {

					// // No matter whether the callee is inside current translation unit
					// // or not, we only care about its name to avoid digging into the
					// // function call tree.
					// string calleeName;
					// if (calledFunc->getGlobalIdentifier().length() != 0)
					// 	calleeName = calledFunc->getGlobalIdentifier();
					// else calleeName = calledFunc->getName().str();
					// FunctionNode::FunctionWaitlist.insert(calleeName); 
					// Greate! the called func is inside current translation unit.
					if (!calledFunc->isDeclaration()) {
						if (calledFunc->isIntrinsic() || !calledFunc->getSubprogram() ||
								isValidDebugInfo(calledFunc->getSubprogram()) != 1) {
							string calleeName;
							if (calledFunc->getGlobalIdentifier().length() != 0)
								calleeName = calledFunc->getGlobalIdentifier();
							else calleeName = calledFunc->getName().str();
							ExternalFunctionWaitlist.insert(calleeName);
						}
						else if (!calledFunc->getSubprogram()->getName().empty()){
							size_t next_node_idx = getFunctionHash(calledFunc);
							// printf("build internal relation %lld call %lld\n", getFunctionHash(func), next_node_idx);
							// dg->print_mtx.lock();
							// printf("[%d] caller function %p ---> %s\n", ms->thread_idx, func, this->name.c_str());
							// printf("[%d] callee function %p ---> %s\n", ms->thread_idx, calledFunc, calledFunc->getName().str().c_str());
							// printf("[%d] callee function 			%s, %d\n", calledFunc->getSubprogram()->getFilename().str().c_str(), calledFunc->getSubprogram()->getLine());
							// calledFunc->getSubprogram()->print(llvm::errs(), nullptr);
							// printf("[%d] callee idx ---> %llu\n", ms->thread_idx, next_node_idx);
							// dg->print_mtx.unlock();
							InternalFunctionWaitlist.insert(next_node_idx);
						}
						// // call_depth++;
						// FunctionNode *nextN = getFunctionNode(calledFunc);
						// // call_depth--;
						// if (nextN) {
						// 	dg->FunctionNodeGraph_Funcdep[this].insert(nextN);
						// }
					}

					// Otherwise we only store it's name for further inter-procedural search
					else {
						string calleeName;
						if (calledFunc->getGlobalIdentifier().length() != 0)
							calleeName = calledFunc->getGlobalIdentifier();
						else calleeName = calledFunc->getName().str();
						ExternalFunctionWaitlist.insert(calleeName); 
					}
				}
			}
		}
	}
	gettimeofday(&end, NULL);
	// printf("[function's funcs summary] time : %d secs\n", end.tv_sec - start.tv_sec);
}

size_t getFunctionNode(Function *F, ModuleSummary *ms, DependencyGraph *dg) {
	if (F->isIntrinsic() || !F->getSubprogram()) return 0;
	if (isValidDebugInfo(F->getSubprogram()) != 1) return 0;

	// Here we only care about functions with original names
	// Closure / defined via macro / generated debuginfo would be ignored.
	if (F->getSubprogram()->getName().empty()) return 0;
	// max_call_depth = std::max(max_call_depth, call_depth);
	// errs() << "[DEBUG getFunctionNode] Depth: " << call_depth << "\n";
	// errs() << "[DEBUG getFunctionNode] Max Depth: " << max_call_depth << "\n";

	size_t node_idx = getFunctionHash(F);

    if (ms->FuncNodeList.count(node_idx)) {
		// FunctionNode *ret = ms->FuncNodeList[node_idx];
		return node_idx;
	}
        
	// errs() << "Create node_idx = " << node_idx << " file_path = " << getFullPath(F->getSubprogram()->getFilename().str(), F->getSubprogram()->getDirectory().str()) 
	// 						<< " line = " << F->getSubprogram()->getLine() << "\n";

	FunctionNode *FN = createFuncNode(F, node_idx, ms, dg);
	// dg->print_mtx.lock();
	// printf("[%d] new function name ---> %s\n", ms->thread_idx, FN->name.c_str());
	// printf("[%d] new function idx ---> %llu\n", ms->thread_idx, node_idx);
	// dg->print_mtx.unlock();
	ms->FuncNodeList[node_idx] = FN;
	ms->FuncNodeToHash[FN] = node_idx;
	ms->newFuncNodes[node_idx] = FN;
	return node_idx;
}

std::string getName(Function *F) {
	DISubprogram *SP = F->getSubprogram();
	std::string identifier = SP->getLinkageName().str();
	std::string name = SP->getName().str();

	std::string final_name = identifier.size() ? identifier : name;
	return final_name;
}

size_t FunctionNode::getDescriptionHash() {
	if (g_vars.use_prototype_rep)
		return (size_t) llvm::hash_combine(source_file_path, prototypeRep, getKind());
	else 
		return (size_t) llvm::hash_combine(source_file_path, getName(func));
}

size_t getFunctionHash(Function *F) {
	DISubprogram *SP = F->getSubprogram();
	assert(SP && SP->getLine() && (!SP->getFilename().empty()) && "[getFunctionHash] Not valid Function in file.");

	std::string full_path = getBasePath(SP->getFilename().str(), SP->getDirectory().str());
	std::string final_name = getName(F);
	// printf("[getFunctionHash F=>%p, SP=>%p] path = %s, line = %d, final_name = %s\n",
	// 	F, SP, full_path.c_str(), SP->getLine(), final_name.c_str());

	return (size_t) llvm::hash_combine(full_path, SP->getLine(), final_name);
}

FunctionNode *createFuncNode(Function *F, size_t hash_idx, 
					ModuleSummary *ms, DependencyGraph *dg) {
	DISubprogram *SP = F->getSubprogram();
	assert(SP && SP->getLine() && (!SP->getFilename().empty()) && "[createFuncNode] Not valid Function in file.");

	std::string name = SP->getName().str();
	std::string base_path = getBasePath(SP->getFilename().str(), SP->getDirectory().str());

	FunctionNode *FN = new FunctionNode(F, name, base_path, SP->getLine(), hash_idx, ms, dg);

	return FN;
}