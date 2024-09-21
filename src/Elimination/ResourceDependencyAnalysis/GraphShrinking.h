#ifndef __GRAPHSHRINKING_H
#define __GRAPHSHRINKING_H

#include <stack>
#include <vector>
#include <unordered_map>
#include <unordered_set>

#include "DiffTextSplit.h"
#include "ProjectRepresentation.h"
#include "GraphGeneration/GeneralNode.h"

class ShrinkedGraph {
public:
    ShrinkedGraph() {}
    // ShrinkedGraph(unsigned _size) {
    //     node_num = _size;
    //     shrinked_node_num = 0;
    // }

    // APIs for main purpose.
    void buildShrinkedGraph( DependencyGraph * dgraph, 
        std::unordered_map<GeneralNode *, std::unordered_set<DiffCodeBlock *> > &NodeToDCBs);
    void nodeMatching(ShrinkedGraph * sgraph);
    void violatedAppliedCBDetect(ShrinkedGraph * sgraph);
    void graphMerging(ShrinkedGraph * sgraph);
    void emitSuggestions(std::string output_path);

    std::vector<std::string> debug_outputs;

private:
    class Node {
    public:
        Node(unsigned int _sl, unsigned int _el, GeneralNode *_gnode, 
            DiffCodeBlock *_DCB, ShrinkedGraph *_sgraph) : start_line(_sl), end_line(_el), 
                gnode(_gnode), sgraph(_sgraph), DCB(_DCB) {}

        std::string debug_print() {
            // printf("[Node] file %s, start %d, end %d\n", gnode->source_file_path.c_str(), start_line, end_line);
            return "[Node] " + gnode->name + " " +  gnode->source_file_path + " " + to_string(start_line) + "-" + to_string(end_line);
        }

        GeneralNode *gnode;
        DiffCodeBlock *DCB;
        ShrinkedGraph *sgraph;

        unsigned int start_line;
        unsigned int end_line;
    };

    class MultiStatNode {
    public:
        MultiStatNode(Node *_n1, Node *_n2) : 
            current_node(_n1), mirror_node(_n2), has_mirror(true) {}

        MultiStatNode(Node *_n1) :
            current_node(_n1), mirror_node(nullptr), has_mirror(false) {}

        MultiStatNode(Node *_n1, bool _hm) :
            current_node(_n1), mirror_node(nullptr), has_mirror(_hm) {}

        Node *current_node;
        bool has_mirror;
        Node *mirror_node;
    };

    struct hashFunction {
        size_t operator()(const std::pair<GeneralNode *, int> &x) const {
            return (size_t) x.first << 8 | x.second;
        }
    };

    static std::unordered_map<DiffCodeBlock *, std::vector<Node *> > DCBToNodes;
    static std::unordered_map<Node *, Node *> matching_nodes;
    static std::unordered_set<Node *> unremovable_nodes;

    bool doviolatedAppliedCBDetect(Node *node_a, Node *node_b, 
                    ShrinkedGraph * sgraph, std::unordered_set<Node *> &vis);
    void doGraphMerging(Node *node_a, Node *node_b, ShrinkedGraph * sgraph, 
                            int pre_idx, std::unordered_set<Node *> &vis);
    void recursiveBuilding(GeneralNode *cur_gnode, int prev_node_idx,
            std::unordered_map<GeneralNode *, std::unordered_set<DiffCodeBlock *> > &NodeToDCBs,
            DependencyGraph * dgraph);
    void tarjan(int v);
    void eliminateCycle();
    void doClassifyGraph(int u);
    void classifyGraph();
    void doTopologySort(int class_id);
    void topologySort();
    void showResults(std::string output_path);

    // std::unordered_set<std::pair<GeneralNode *, int>, hashFunction> visited_nodes;
    std::unordered_set<GeneralNode *> visited_nodes;
    std::vector<Node *> IndexToNodePtr;
    std::unordered_map<Node *, int> NodePtrToIndex;
    std::unordered_set<DiffCodeBlock *> DCBs;
    
    std::unordered_set<Node *> nodes;
    std::vector<std::unordered_set<int> > edges;
    std::vector<std::unordered_set<int> > shrinked_directed_edges;
    std::vector<std::unordered_set<int> > shrinked_edges;
    std::unordered_map<GeneralNode *, std::vector<Node *> > GnodeToNode;
    std::unordered_map<Node *, MultiStatNode *> NodeToMSN;
    std::unordered_map<int, std::vector<GeneralNode *> > path_along;

    std::vector<std::unordered_set<int> > merged_edges;
    std::unordered_map<MultiStatNode *, unsigned> MSNToIndex;
    std::unordered_map<unsigned, MultiStatNode *> IndexToMSN;

    std::vector<int> dfn;
    std::vector<int> low;
    std::vector<bool> in_tarjan_stack;
    std::stack<int> tarjan_stack;
    std::vector<int> color;
    std::vector<int> inDegree;
    std::vector<std::unordered_set<int> > node_groups;
    std::vector<std::unordered_set<int> > classified_nodes;
    std::vector<std::vector<int> > topsort_results;
    std::vector<int> top_order;

    std::vector<bool> visited;

    int timestamp;
    int bit_wise;

    unsigned multi_stat_node_num;
    unsigned class_num;
    unsigned node_num;
    unsigned shrinked_node_num;
    unsigned scc_num;
};

#endif
