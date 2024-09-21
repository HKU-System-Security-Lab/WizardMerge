#ifndef GENERALNODE_H
#define GENERALNODE_H

#include "Project.h"
#include "Utils.h"
#include "DependencyGraph.h"

class DependencyGraph;

class GeneralNode {
public:

	enum NodeKind {
        NK_Type,
        NK_GlobalVar,
        NK_Function
    };

    GeneralNode(std::string _name, std::string _source_path,
			 unsigned int _line, size_t _hash_val, NodeKind K, DependencyGraph *_dg) :
        name(_name), source_file_path(_source_path), start_line(-1), 
			debug_line(_line), hash_val(_hash_val), _kind(K), end_line(-1), dg(_dg) {}

	NodeKind getKind() const { return _kind; }
	virtual std::string debug_print() = 0;
	virtual size_t getDescriptionHash() = 0;

    std::string name;
	std::string mangled_name;
	std::string source_file_path;
	unsigned int start_line;
	unsigned int end_line;
	unsigned int debug_line;
	size_t hash_val;
	DependencyGraph *dg;
private:
	const NodeKind _kind;
};



#endif
