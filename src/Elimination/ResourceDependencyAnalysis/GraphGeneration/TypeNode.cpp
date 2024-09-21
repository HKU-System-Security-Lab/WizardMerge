#include "TypeNode.h"
#include "Utils.h"

#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/DebugInfo.h>

std::string getName(DIType *Ty) {
	std::string identifier = "";
	// return Ty->getName() ? Ty->getName().str() : "";
	std::string name = Ty->getName().str();
	if (auto *T = dyn_cast<DICompositeType>(Ty))
		identifier = T->getIdentifier().str();
	assert((identifier.size() || name.size()) && "[getName] Not valid Type in name.");
	std::string final_name = identifier.size() ? identifier : name;
	return final_name;
}

void TypeNode::handleType(DIType *Ty) {
	// errs() << "[DEBUG] collectTypes" << "\n";
	TyPtr = Ty;
	mangled_name = "";
	if (auto *T = dyn_cast<DICompositeType>(Ty))
	   	mangled_name = T->getIdentifier().str();

	// Sometimes the type is generated but we still need to consider,
	// thus, we create a virtual file node for them.
	assert(!TyPtr->getFilename().empty());
	// if (TyPtr->getFilename().empty()) 
	// 	TypeNode::fileMapsToTypeNodes["VIRTUAL_FILE"].insert(this);
	// else {
	ms->fileMapsToNodes[source_file_path].insert(hash_val);
	ms->newNodeToFile[dyn_cast<GeneralNode>(this)] = source_file_path;
	// }
			
}

size_t TypeNode::getDescriptionHash() {
	return (size_t) llvm::hash_combine(source_file_path, getName(TyPtr), getKind());
}

// Only subroutineType or compositeType can get in this function
void flattenComplexType(DIType *Ty, ModuleSummary *ms, DependencyGraph *dg) {
	if (ms->TypeNodeCache.count(Ty) && !ms->TypeNodeCache[Ty].empty()) {
		// errs() << "[DEBUG] flattenComplexType Ty = " << Ty << " found TypeNodeCache\n";
		return;
	}

	if (auto *T = dyn_cast<DISubroutineType>(Ty)) {
		
		for (auto *subTy : T->getTypeArray()) {
			if (!subTy) continue;

			unordered_set<size_t> subResult;
			bool valuableResult = getTypeNode(subTy, subResult, ms, dg);

			if (valuableResult) {
				// errs() << "[DEBUG] flattenComplexType get valuable result Ty = " << Ty << "\n";
				ms->TypeNodeCache[Ty].insert(subResult.begin(), subResult.end());
			}
 
		}
	}

	if (auto *T = dyn_cast<DICompositeType>(Ty)) {
		for (DINode *e : T->getElements()) {
			// Only care about Type member.
			if (DIType * subTy = dyn_cast<DIType>(e)) {
				unordered_set<size_t> subResult;
				bool valuableResult = getTypeNode(subTy, subResult, ms, dg);

				if (valuableResult) {
					// errs() << "[DEBUG] flattenComplexType get valuable result Ty = " << Ty << "\n";
					ms->TypeNodeCache[Ty].insert(subResult.begin(), subResult.end());
				}
			}
		}
	}
}

DIType *getBaseDIType(DIType *Ty) {
	// errs() << "[DEBUG] getBaseDIType" << "\n";

	// For DIBasicType and DIStringType,
	// we don't need to parse them later.
	if (auto *t = dyn_cast<DIBasicType>(Ty)) {
		return NULL;
	}
	if (auto *t = dyn_cast<DIStringType>(Ty)) {
		return NULL;
	}
	if (auto *t = dyn_cast<DISubroutineType>(Ty)) {
		return Ty;
	}
	if (auto *t = dyn_cast<DICompositeType>(Ty)) {
		int res = isValidDebugInfo(Ty);
		// We ignore all forwarded declare, because we only
		// care about definition.
		if (res == -1 || t->isForwardDecl()) return NULL;
		return Ty;
	}
	if (auto *t = dyn_cast<DIDerivedType>(Ty)) {
		int res = isValidDebugInfo(Ty);
		if (res == -1 || t->getBaseType() == NULL) return NULL;
		// if (res == 1 && Ty->getTag() == llvm::dwarf::DW_TAG_typedef) return Ty;
		if (res == 1 && Ty->getTag() == 22) return Ty;
		
		return getBaseDIType(t->getBaseType());
	}
	// errs() << "[WARNING] unhandled type\n";
	return NULL;
}

size_t getTypeHash(DIType *Ty) {
	assert(Ty->getLine() && (!Ty->getFilename().empty()) && "[getTypeHash] Not valid Type in file.");

	std::string base_path = getBasePath(Ty->getFilename().str(), Ty->getDirectory().str());
	std::string final_name = getName(Ty);

	return (size_t) llvm::hash_combine(base_path, Ty->getLine(), final_name);
}

TypeNode *createTypeNode(DIType * Ty, size_t hash_idx, DependencyGraph *dg, ModuleSummary *ms) {
	assert(Ty->getLine() && (!Ty->getFilename().empty()) && "[createTypeNode] Not valid Type in file.");

	std::string final_name = Ty->getName().str().size() ? Ty->getName().str() : "*";
	std::string base_path = getBasePath(Ty->getFilename().str(), Ty->getDirectory().str());

	TypeNode *TN = new TypeNode(Ty, final_name, base_path, Ty->getLine(), hash_idx, dg, ms);
	// GeneralNode *GN = dyn_cast<GeneralNode>(TN);
	// errs() << "TN = " << TN << " CASTED TN = " << GN << "\n";
	// errs() << "TN->line = " << TN->debug_line << " casted Tn->line = " << GN->debug_line << "\n";
	return TN;
}