#ifndef TYPENODE_H
#define TYPENODE_H

#include "Project.h"
#include "Utils.h"
#include "GeneralNode.h"
#include "ModuleSummary.h"
#include "DependencyGraph.h"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugInfo.h>
#include <iostream>
#include <mutex>

class ModuleSummary;
class DependencyGraph;

class TypeNode : public GeneralNode {
public:

	TypeNode(DIType *DIT, std::string _name, std::string _source_path,
			 unsigned int _line, size_t _hash_val, DependencyGraph *_dg, ModuleSummary *_ms) : 
		GeneralNode(_name, _source_path, _line, _hash_val, NK_Type, _dg)
	{
		ms = _ms;
        handleType(DIT);
    }

	size_t getDescriptionHash();

	std::string debug_print() {
		return "[TypeNode] " + name + " " + source_file_path + " " + to_string(debug_line);
	}

	static bool classof(const GeneralNode *GN) {
		return GN->getKind() == NK_Type;
	}

	ModuleSummary *ms;
	DIType *TyPtr;

private:
	void handleType(DIType *Ty);

};

DIType *getBaseDIType(DIType *Ty);
size_t getTypeHash(DIType *Ty);
void flattenComplexType(DIType *Ty, ModuleSummary *ms, DependencyGraph *dg);
TypeNode *createTypeNode(DIType * Ty, size_t hash_idx, DependencyGraph *dg, ModuleSummary *ms);

// bool typenode_sorter(TypeNode *A, TypeNode *B) {
// 	return A->debug_line < B->debug_line;
// }

template <typename T1>
bool getTypeNode(T1 *Ty, unordered_set<size_t> &ret_set, 
					ModuleSummary *ms, DependencyGraph *dg, int from_outside = 0) {
	// errs() << "[DEBUG] getTypeNode Ty = " << Ty << "\n";
	// Ty->dump();
	
	DIType *convertedTy = dyn_cast<DIType>(Ty);
	if (!convertedTy) {
		// errs() << "[WARNING getTypeNode] forced convertion fails\n";
		return false;
	}

	// stop here, cost 49.680

	if (ms->IgnoredTypeNode.count(convertedTy)) {
		return false;
	}

	// Check whether the current Ty has been cached.
	// We need overall caching because getBaseDIType is also time-consuming.
	if (ms->TypeNodeCache.count(convertedTy) && !ms->TypeNodeCache.empty()) {
		// errs() << "[DEBUG] getTypeNode Typtr = " << TyPtr << " found TypeNodeCache\n";
		ret_set = ms->TypeNodeCache[convertedTy];
		return true;
	}

	// stop here, cost 1:00.97

	// if (from_outside) {
	// 	// errs() << "[getTypeNode] getTypeNode Inefficient Detected!\n";
	// 	convertedTy->dump();
	// }
	DIType *TyPtr = getBaseDIType(convertedTy);
	// stop here, cost 2:57.45

	// if (from_outside)
	// 	return false;

	// After here, only the following types are accepted:
	//		1. all DISubroutineType.
	// 		2. DICompositeType confirmed to be in the current project directory.
	//		3. DICompositeType with unkown source.
	//		4. DIDerivedType confirmed to be in the current project directory.
	if (!TyPtr) {
		// errs() << "[DEBUG] getTypeNode returns because TyPtr is NULL\n";
		ms->IgnoredTypeNode.insert(TyPtr);
		if (TyPtr != convertedTy)
			ms->IgnoredTypeNode.insert(convertedTy);
		return false;
	}
	// errs() << "[DEBUG] getTypeNode baseTy = ";
	// TyPtr->dump();

	if (ms->IgnoredTypeNode.count(TyPtr)) {
		// errs() << "[DEBUG] getTypeNode Found in ignored node list.\n";
		// Ty->dump();
		if (TyPtr != convertedTy)
			ms->IgnoredTypeNode.insert(convertedTy);
		return false;
	}
			
	// errs() << "[DEBUG] getTypeNode Base type get\n";
	// TyPtr->dump();

	// If the TyPtr has been cached, then we get the data from cache.
	// Notice that even for the same DIType, they may not be recognized in 
	// various modules because we cache them by the pointer address value.
	if (ms->TypeNodeCache.count(TyPtr) && !ms->TypeNodeCache.empty()) {
		// errs() << "[DEBUG] getTypeNode Typtr = " << TyPtr << " found TypeNodeCache\n";
		ret_set = ms->TypeNodeCache[TyPtr];

		// Update outer type (i.e., Ty) as well.
		// errs() << "[DEBUG] getTypeNode build caching for Ty = " << convertedTy << "\n";
		if (TyPtr != convertedTy)
			ms->TypeNodeCache[convertedTy].insert(ret_set.begin(), ret_set.end());
		return true;
	}

	else {
		// errs() << "[DEBUG] getTypeNode Ty = " << Ty << " you arrived here!" << "\n";
		// errs() << "[DEBUG] getTypeNode ready for handling named types!\n";
		// After here, only the following types are accepted:
		// 		1. DICompositeType confirmed to be in the current project directory.
		//		2. DIDerivedType confirmed to be in the current project directory.
		if (isa<DISubroutineType>(TyPtr) ||
				(isa<DICompositeType>(TyPtr) && !isUserDefinedType(TyPtr))){
			flattenComplexType(TyPtr, ms, dg);
			if (ms->TypeNodeCache[TyPtr].empty()) {
				// errs() << "[DEBUG] getTypeNode convertedTy =  " << convertedTy << " added to ignored node list.\n";
				ms->IgnoredTypeNode.insert(TyPtr);
				ms->IgnoredTypeNode.insert(convertedTy);
				return false;
			}
			ret_set = ms->TypeNodeCache[TyPtr];
			// Update outer type (i.e., Ty) as well.
			if (TyPtr != convertedTy)
				ms->TypeNodeCache[convertedTy].insert(ret_set.begin(), ret_set.end());
			// errs() << "[DEBUG] getTypeNode build caching for convertedTy = " << convertedTy << "\n";
			return true;
		}
		// If TyPtr has a corresponding TypeNode,
		// we return it without creating duplicately.
		size_t node_idx = getTypeHash(TyPtr);
		// errs() << "[DEBUG] getTypeNode TyPtr = " << TyPtr << " node_idx = " << node_idx << "\n";

		TypeNode *currentN = nullptr;

		if (ms->TypeNodeList.count(node_idx)) {
			ret_set.insert(node_idx);
			ms->TypeNodeCache[TyPtr].insert(node_idx);
			ms->TypeNodeCache[convertedTy].insert(node_idx);
			return true;
		}
			
		currentN = createTypeNode(TyPtr, node_idx, dg, ms);
		// ms->newTypeNodes[node_idx] = currentN;
		ms->TypeNodeList[node_idx] = currentN;
		ms->TypeNodeToHash[currentN] = node_idx;

		ret_set.insert(node_idx);
		// Update outer type (i.e., Ty) as well.
		ms->TypeNodeCache[TyPtr].insert(node_idx);
		ms->TypeNodeCache[convertedTy].insert(node_idx);
		// errs() << "[DEBUG] getTypeNode build caching for convertedTy = " << convertedTy << "\n";
		// Retrieve dependency graph for DICompositeType node.
		if (auto *T = dyn_cast<DICompositeType>(TyPtr)) {
			for (DINode *e : T->getElements()) {
				if (DIType * subTy = dyn_cast<DIType>(e)) {
					unordered_set<size_t> subResult;
					bool valuableResult = getTypeNode(subTy, subResult, ms, dg);
					if (valuableResult) {
						for (auto subTN_idx : subResult) {
							ms->TypeNodeGraph[node_idx].insert(subTN_idx);
							// ms->TypeNodeGraphhash[currentN].insert(getTypeHash(subTN->TyPtr));
						}
					}
				}
			}
		}
		// Retrieve dependency graph for DIDerivedType node.
		if (auto *T = dyn_cast<DIDerivedType>(TyPtr)) {
			unordered_set<size_t> subResult;
			bool valuableResult = getTypeNode(T->getBaseType(), subResult, ms, dg);
			if (valuableResult) {
				for (auto subTN_idx : subResult) {
					// ms->TypeNodeGraph[currentN].insert(subTN);
					ms->TypeNodeGraph[node_idx].insert(subTN_idx);
					// ms->TypeNodeGraphhash[currentN].insert(getTypeHash(subTN->TyPtr));
				}
			}
		}

		// dg->mtx_access.lock();
		// if (currentN->name == "_ZTSN8solidity8frontend10MemberList6MemberE") {
		// 	printf("[idx = %d getTypeNode] create new node ", ms->thread_idx);
		// 	currentN->debug_print();
		// 	TyPtr->dump();
		// 	for (auto *TN : ms->TypeNodeGraph[currentN]) {
		// 		printf("        [SubTN] %ld: ", getTypeHash(TN->TyPtr));
		// 		TN->debug_print();
		// 		TN->TyPtr->dump();
		// 	}
		// }
		
		// dg->mtx_access.unlock();
	}
	

	return true;
}

#endif