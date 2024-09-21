#ifndef PROJECTrEPRESENTATION_H
#define PROJECTrEPRESENTATION_H

#include <llvm/IR/Module.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <unordered_set>
#include <vector>
#include <iostream>
#include <algorithm>
#include "GraphGeneration/GeneralNode.h"

enum RANGE_TYPE {NONE_RANGE, OTHERS, COMPOSITE_TYPE, ALIAS, GLOABL_VAR, FUNCTION};
// enum RANGE_TYPE {NONE_RANGE, OTHERS, COMPOSITE_TYPE, GLOABL_VAR, FUNCTION_PROTO, FUNCTION_BODY};
enum DIFF_TYPE {NONE_DIFF, GENERATED, APPLYA, APPLYB, BOTH, CONFLICT};
enum INCLUDE_RELATION {INCLUDE, NOT_INCLUDE, PARTLY_INCLUDE};

typedef struct AbstractLine_s {
    unsigned L, R;
    GeneralNode *Node;
    AbstractLine_s(unsigned l, unsigned r, GeneralNode *p) : L(l), R(r), Node(p) {}
} AbstractLine;


class PreliminaryCodeBlock {
public:
    PreliminaryCodeBlock(bool isVariantA, std::string filename, 
                const unsigned start_line, const unsigned length) : 
        _isVariantA(isVariantA), _filename(filename), 
            _start_line(start_line), _length(length),
                _end_line(_start_line + _length - 1) {}

    bool isFromVariantA() {return _isVariantA;}
    virtual std::string getSourceFileName() const {return _filename;}
    virtual const unsigned getStartLine() const {return _start_line;}
    virtual const unsigned getEndLine() const {return _end_line;}
    virtual const unsigned getLength() const {return _length;}

    INCLUDE_RELATION isIncludedBy(const PreliminaryCodeBlock &PCB) {
        if (PCB.getSourceFileName() != _filename) return NOT_INCLUDE;
        if (PCB.getStartLine() > _end_line || PCB.getEndLine() < _start_line) return NOT_INCLUDE;
        if (PCB.getStartLine() <= _start_line && PCB.getEndLine() >= _end_line) return INCLUDE;
        return PARTLY_INCLUDE;
    }

private:
    bool _isVariantA;
    std::string _filename;
    const unsigned _start_line;
    const unsigned _length;
    const unsigned _end_line;
};

class DiffCodeBlock;
class RangeCodeBlock;
class ModifiedFileInVariant;

class DiffCodeBlock : public PreliminaryCodeBlock {
public:
    DiffCodeBlock(bool isVariantA, std::string filename, ModifiedFileInVariant *MFV,
                const unsigned start_line, const unsigned length, DIFF_TYPE dt) : 
        _isClassified(false), _mirror(NULL), _parent(NULL), _diff_type(dt), _MFV(MFV), 
            PreliminaryCodeBlock(isVariantA, filename, start_line, length) {}

    void setAsClassified() {_isClassified = true;}
    void setMirror(DiffCodeBlock *DCB) {_mirror = DCB;}
    void setParent(RangeCodeBlock *RCB) {_parent = RCB;}

    ModifiedFileInVariant *getModifiedFile() {return _MFV;}
    bool isClassified() {return _isClassified;}
    DIFF_TYPE getDiffType() {return _diff_type;}
    DiffCodeBlock *getMirror() {return _mirror;}
    RangeCodeBlock *getParent() {return _parent;}

    std::string debug_print() {
        std::string t = "";
        if (_diff_type == APPLYA) t = "VA";
        if (_diff_type == APPLYB) t = "VB";
        if (_diff_type == CONFLICT) t = "CONFLICT";
        return "[DCB] " + getSourceFileName() + " " + to_string(getStartLine()) + "-" + to_string(getEndLine()) + " " + t;
    }

    std::unordered_set<GeneralNode *> nodes;

private:
    ModifiedFileInVariant *_MFV;
    bool _isClassified;
    DiffCodeBlock *_mirror;
    RangeCodeBlock *_parent;
    DIFF_TYPE _diff_type;
};

class RangeCodeBlock : public PreliminaryCodeBlock {
public:
    RangeCodeBlock(bool isVariantA, std::string filename, ModifiedFileInVariant *MFV, std::string range_name,
                const unsigned start_line, const unsigned length, RANGE_TYPE rt) : 
        _mirror(NULL), _range_type(rt), _depth(0), _MFV(MFV), _range_name(range_name),
            PreliminaryCodeBlock(isVariantA, filename, start_line, length) {}

    RANGE_TYPE getRangeType() {return _range_type;}
    // RangeCodeBlock *getMirror() {return _mirror;}
    unsigned getDepth() {return _depth;}
    ModifiedFileInVariant *getModifiedFile() {return _MFV;}
    std::string getName() { return _range_name; }
    
    void setDepth(unsigned dep) {_depth = dep;}
    // void setMirror(RangeCodeBlock *RCB) {_mirror = RCB;} 

    void insertDiffCodeBlock(DiffCodeBlock *DCB) {
        _include_diff_code_blocks.insert(DCB);
    }

    void removeDiffCodeBlock(DiffCodeBlock *DCB) {
        _include_diff_code_blocks.erase(DCB);
    }

private:
    ModifiedFileInVariant *_MFV;
    RangeCodeBlock *_mirror;
    RANGE_TYPE _range_type;
    std::string _range_name;
    std::set<DiffCodeBlock *> _include_diff_code_blocks;
    unsigned _depth;
};

class FunctionBodyBlocksGroup {
public:
    FunctionBodyBlocksGroup() = default;

    void addCodeBlock(DiffCodeBlock *DCB) {_body_diff_codeblocks.push_back(DCB);}

    void mergeFunctionBodies();

private:
	std::vector<DiffCodeBlock *> _body_diff_codeblocks;
};

class ModifiedFileInVariant {
public:
    ModifiedFileInVariant(std::string relative_filename, std::string filename, int _lines, bool isVariantA) :
        _relative_filename(relative_filename), _filename(filename), lines(_lines), _isVariantA(isVariantA) {}

    void classifyDiffCodeBlocks();
	void castDepthForRangeCodeBlocks();
    bool readIRFromFile(std::string IRFilePath);

    bool isFromVariantA() {return _isVariantA;}
    ModifiedFileInVariant *getMirror() {return _mirror;}
    std::string getSourceFileName() const {return _filename;}
    std::string getRelativeFileName() const {return _relative_filename;}
    llvm::Module *getLLVMModule() {return NULL;}
    int getLineNumber() {return lines;}

    void setMirror(ModifiedFileInVariant *DCB) {_mirror = DCB;}

    void addDiffCodeBlock(DiffCodeBlock *DCB) {_diff_code_blocks.push_back(DCB);}
    void addRangeCodeBlock(RangeCodeBlock *RCB) {_range_code_blocks.push_back(RCB);}

    void getMergedGroups(std::vector<FunctionBodyBlocksGroup *> &G);

    void appendClassifiedDCBs(std::vector<DiffCodeBlock *> &G);

    void getRangeCodeBlocks(std::vector<RangeCodeBlock *> &G);
    std::vector<DiffCodeBlock *> _diff_code_blocks;
    std::vector<RangeCodeBlock *> _range_code_blocks;
    std::vector<AbstractLine> _mapped_ranges;

private:
	// std::vector<RangeCodeBlock *> _range_code_blocks;
	// std::vector<DiffCodeBlock *> _diff_code_blocks;
	
	std::vector<DiffCodeBlock *> _classified_diff_code_blocks;
	std::vector<FunctionBodyBlocksGroup *> _classified_func_body_groups;

	ModifiedFileInVariant *_mirror;

    std::string _filename;
    std::string _relative_filename;

    // These two are just for maintaining the llvm module
    llvm::LLVMContext ctx;
    llvm::SMDiagnostic smd;

    std::unique_ptr<llvm::Module> _M;

	bool _isVariantA;

    int lines;
};

#endif