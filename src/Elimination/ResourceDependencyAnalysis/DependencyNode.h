#ifndef __DEPENDENCYNODE_H
#define __DEPENDENCYNODE_H

#include "ProjectRepresentation.h"

class DependencyNode {
public:
    DependencyNode(DiffCodeBlock * DCB, bool isRoot) :
         _content(DCB), _isRoot(isRoot) {}

    void addKid(DependencyNode *DN) {_kids.push_back(DN);}
	void addParent(DependencyNode *DN) {_parents.push_back(DN);}
    
    void setRoot(bool isRoot) {_isRoot = isRoot;}

private:
    std::vector<DependencyNode *> _parents;
    std::vector<DependencyNode *> _kids;

    DiffCodeBlock * _content;

    bool _isRoot;
};

#endif