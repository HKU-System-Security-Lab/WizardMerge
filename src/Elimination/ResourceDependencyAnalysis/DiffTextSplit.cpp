#include "DiffTextSplit.h"
#include <unistd.h>

SegmentTree *global_st;

int SegmentTree::getLeftKidIdx(int idx) {
    return (idx << 1) + 1;
}

int SegmentTree::getRightKidIdx(int idx) {
    return (idx << 1) + 2;
}

void SegmentTree::buildSegmentTree(int idx, unsigned l, unsigned r) {
    if (l == r) {
        // printf("[buildSegmentTree] l == r == %d, idx = %d, Node = %p\n", l, idx, data[l].Node);
        tree[idx].insert(data[l].Node);
        return;
    }

    int l_idx = getLeftKidIdx(idx);
    int r_idx = getRightKidIdx(idx);
    int mid = l + ((r - l) >> 1);

    buildSegmentTree(l_idx, l, mid);
    buildSegmentTree(r_idx, mid + 1, r);

    tree[idx].insert(tree[l_idx].begin(), tree[l_idx].end());
    tree[idx].insert(tree[r_idx].begin(), tree[r_idx].end());
}

void SegmentTree::updateSegmentTree(int idx, int l, int r, 
                        int ul, int ur, GeneralNode * delta) {
    if (ul <= l && ur >= r) {
        tree[idx].clear();
        lazy[idx] = delta;
        tree[idx].insert(delta);
        // for (auto *node : tree[idx]) {
        //     printf("[updateSegmentTree] idx=%d, l=%d, r=%d, ul=%d, ur=%d\n", idx, l, r, ul, ur);
        //       printf("    [updateSegmentTree] Node named %s in %s starts from %d, ends to %d\n", 
        //             node->name.c_str(), node->source_file_path.c_str(), node->start_line, node->end_line);
        // }
        return;
    }

    int mid = l + ((r - l) >> 1);
    int l_idx = getLeftKidIdx(idx);
    int r_idx = getRightKidIdx(idx);

    // If current node has been modified, push down to sub trees.
    if (lazy[idx]) {
        tree[l_idx].clear();
        tree[r_idx].clear();
        tree[l_idx].insert(lazy[idx]);
        tree[r_idx].insert(lazy[idx]);

        lazy[l_idx] = lazy[r_idx] = lazy[idx];
        lazy[idx] = NULL;
    }

    if (ul <= mid){
        updateSegmentTree(l_idx, l, mid, ul, ur, delta);
    }
    if (ur > mid){
        updateSegmentTree(r_idx, mid+1, r, ul, ur, delta);
    }

    tree[idx].clear();
    tree[idx].insert(tree[l_idx].begin(), tree[l_idx].end());
    tree[idx].insert(tree[r_idx].begin(), tree[r_idx].end());
    // printf("[updateSegmentTree] idx=%d, l=%d, r=%d, ul=%d, ur=%d\n", idx, l, r, ul, ur);
    // for (auto *node : tree[idx]) {
    //     printf("    [updateSegmentTree] Node named %s in %s starts from %d, ends to %d\n", 
    //             node->name.c_str(), node->source_file_path.c_str(), node->start_line, node->end_line);
    // }
}

void SegmentTree::querySegmentTree(int idx, int l, int r, 
                        int ql, int qr, std::unordered_set<GeneralNode *> &ans) {

    if (l >= ql && r <= qr) {
        // printf("[querySegmentTree] idx=%d, l=%d, r=%d, ql=%d, qr=%d\n", idx, l, r, ql, qr);
        // for (auto *node : tree[idx]) {
        //       printf("    [querySegmentTree] Node named %s in %s starts from %d, ends to %d\n", 
        //             node->name.c_str(), node->source_file_path.c_str(), node->start_line, node->end_line);
        // }
        ans.insert(tree[idx].begin(), tree[idx].end());
        return;
    }

    int l_idx = getLeftKidIdx(idx);
    int r_idx = getRightKidIdx(idx);
    int mid = l + ((r - l) >> 1);
    // sleep(5);
    
    // if "lazy" is tagged to the current node, update sub trees.
    if (lazy[idx]) {
        tree[l_idx].clear();
        tree[r_idx].clear();
        tree[l_idx].insert(lazy[idx]);
        tree[r_idx].insert(lazy[idx]);

        lazy[l_idx] = lazy[r_idx] = lazy[idx];
        lazy[idx] = NULL;
    }

    if (ql <= mid){
        querySegmentTree(l_idx, l, mid, ql, qr, ans);
    }
    if (qr > mid){
        querySegmentTree(r_idx, mid+1, r, ql, qr, ans);
    }
    return;
}

void SegmentTree::buildSegmentTreeByLines() {
    for (auto line : data) {
        updateSegmentTree(0, 0, _data_size, line.L, line.R, line.Node);
    }
}

void SegmentTree::query(int ql, int qr, std::unordered_set<GeneralNode *> &ans) {
    querySegmentTree(0, 0, _data_size, ql, qr, ans);
}

void DCB_NodeMapping(std::vector<AbstractLine> &GeneralNodes, 
        std::vector<DiffCodeBlock *> &DCBs, 
        std::unordered_map<GeneralNode *, std::unordered_set<DiffCodeBlock *> > &GNToDCBs, 
        int max_line) {
    if (GeneralNodes.size() == 0) return;
    global_st = new SegmentTree(GeneralNodes, max_line);

    for (auto *DCB : DCBs) {
        bool is_empty_DCB = false;
        if (DCB->getStartLine() > DCB->getEndLine()) continue;
        // even for empty DCB, we still count it and hope can find a matching node for it further.
        // if (DCB->getStartLine() > DCB->getEndLine()) {
        //     is_empty_DCB = true;
        //     global_st->query(DCB->getEndLine(), DCB->getEndLine(), DCB->nodes);
        // }
            
        // else        
            // global_st->query(DCB->getStartLine(), DCB->getEndLine(), DCB->nodes);
        global_st->query(DCB->getStartLine(), DCB->getEndLine(), DCB->nodes);
        
        if (!is_empty_DCB)
            for (auto *node : DCB->nodes) {
                // printf("    [DEBUG] Node named %s in %s starts from %d, ends to %d\n", 
                        // node->name.c_str(), node->source_file_path.c_str(), node->start_line, node->end_line);
                // assert(GNToDCBs.count(node) == 0);
                GNToDCBs[node].insert(DCB);
            }
        // else 
        //     // for (auto *node : DCB->nodes) {
        //     for (auto it = DCB->nodes.begin(); it != DCB->nodes.end(); ) {
        //         // printf("    [DEBUG] Node named %s in %s starts from %d, ends to %d\n", 
        //                 // node->name.c_str(), node->source_file_path.c_str(), node->start_line, node->end_line);
        //         // assert(GNToDCBs.count(node) == 0);

        //         // Heuristic for one node is tagged, while the matching node exists but not tagged.
        //         if (DCB->getStartLine() == (*it)->start_line)
        //             it = DCB->nodes.erase(it);
        //         else {
        //             GNToDCBs[*it].insert(DCB);
        //             it++;
        //         }
        //     }
    }
    // printf("\n\n");

    delete global_st;
    global_st = nullptr;
}