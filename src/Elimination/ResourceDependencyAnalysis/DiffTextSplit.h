#ifndef __DIFFTEXTSPLIT_H
#define __DIFFTEXTSPLIT_H

#include "ProjectRepresentation.h"
#include "GraphGeneration/GeneralNode.h"

class SegmentTree {
public:
    SegmentTree(std::vector<AbstractLine> &_data, int max_line) : data(_data.begin(), _data.end()),
        _data_size(max_line), _tree_max_length(max_line * 4 + 5) {
        lazy.resize(_tree_max_length, NULL);
        tree.resize(_tree_max_length);
        buildSegmentTreeByLines();
    }

    void query(int ql, int qr, std::unordered_set<GeneralNode *> &ans);

private:

    int getLeftKidIdx(int idx);
    int getRightKidIdx(int idx);
    void buildSegmentTree(int idx, unsigned l, unsigned r);
    void querySegmentTree(int idx, int l, int r, 
                        int ql, int qr, std::unordered_set<GeneralNode *> &ans);
    void buildSegmentTreeByLines();
    void updateSegmentTree(int idx, int l, int r, 
                        int ul, int ur, GeneralNode * delta);

    std::vector<std::unordered_set<GeneralNode *> > tree;
    std::vector<GeneralNode *> lazy;
    std::vector<AbstractLine> data;
    unsigned _tree_max_length;
    unsigned _data_size;
};

void DCB_NodeMapping(std::vector<AbstractLine> &GeneralNodes, 
        std::vector<DiffCodeBlock *> &DCBs, 
        std::unordered_map<GeneralNode *, std::unordered_set<DiffCodeBlock *> > &GNToDCBs, 
        int max_line);

#endif