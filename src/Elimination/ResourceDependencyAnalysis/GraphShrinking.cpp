#include "GraphShrinking.h"
#include "GraphGeneration/FunctionNode.h"
#include "GraphGeneration/GlobalVariableNode.h"
#include "GraphGeneration/TypeNode.h"
#include "GraphGeneration/DependencyGraph.h"
#include <queue>

std::unordered_map<DiffCodeBlock *, std::vector<ShrinkedGraph::Node *> > ShrinkedGraph::DCBToNodes;
std::unordered_map<ShrinkedGraph::Node *, ShrinkedGraph::Node *> ShrinkedGraph::matching_nodes;
std::unordered_set<ShrinkedGraph::Node *> ShrinkedGraph::unremovable_nodes;

std::vector<GeneralNode *> path;

bool compare_DCB_by_line(DiffCodeBlock *da, DiffCodeBlock *db) {
    return da->getStartLine() < db->getStartLine();
}

void ShrinkedGraph::recursiveBuilding(GeneralNode *cur_gnode, int prev_node_idx,
    std::unordered_map<GeneralNode *, std::unordered_set<DiffCodeBlock *> > &NodeToDCBs,
    DependencyGraph * dgraph) {

    // If current general node is mapped to DCBs,
    // we need to add it into shrinked graph, and generate
    // an edge from this node to prev node.

    // The node may be split by DCBs, so we need to 
    // store all sub-nodes, and record the first and last ones.
    Node *first_node = nullptr;
    Node *last_node = nullptr;
    if (NodeToDCBs.count(cur_gnode)) {
        // std::cout << banner << cur_gnode->debug_print() << "\n";
        if (GnodeToNode.count(cur_gnode) == 0) {
            std::vector<DiffCodeBlock *> sort_helper(NodeToDCBs[cur_gnode].begin(), NodeToDCBs[cur_gnode].end());
            std::sort(sort_helper.begin(), sort_helper.end(), compare_DCB_by_line);

            Node *prev_new_node = nullptr;
            for (auto *DCB : sort_helper) {
                unsigned int sl = std::max(cur_gnode->start_line, DCB->getStartLine());
                unsigned int el = std::min(cur_gnode->end_line, DCB->getEndLine());
                Node *new_node = new Node(sl, el, cur_gnode, DCB, this);
                errs() << "[recursiveBuilding] new node " << sl << " - " << el << " " << cur_gnode->debug_print() << "\n";
                // printf("[recursiveBuilding] new node %d, filepath = %s, start %d, end %d built\n", 
                    // shrinked_node_num, cur_gnode->source_file_path.c_str(), sl, el);
                GnodeToNode[cur_gnode].push_back(new_node);
                DCBToNodes[DCB].push_back(new_node);
                NodePtrToIndex[new_node] = shrinked_node_num;
                nodes.insert(new_node);
                IndexToNodePtr.push_back(new_node);
                shrinked_node_num++;

                if (shrinked_node_num >= edges.size())
                    edges.resize((int)(edges.size() * 1.6 + 1));

                // For one node with multiple sub-nodes, we need to prebuild
                // internal edges between the sub-nodes.
                // 
                // The key idea is simple: CB with larger line number depends on
                // the CB with smaller line number.
                if (prev_new_node) {
                    // printf("    InsertingA edge from %d - %p\n", NodePtrToIndex[new_node], new_node);
                    // printf("    InsertingA edge to %d - %p\n", NodePtrToIndex[prev_new_node], prev_new_node);
                    // printf("    Inserting A edge from \n");
                    // std::cout << new_node->debug_print() << "\n";
                    // printf("    Inserting A edge to \n");
                    // std::cout << prev_new_node->debug_print() << "\n";
                    // printf("from %d to %d !!! \n", NodePtrToIndex[new_node], NodePtrToIndex[prev_new_node]);
                    edges[NodePtrToIndex[new_node]].insert(NodePtrToIndex[prev_new_node]);
                }
                    
                prev_new_node = new_node;
            }
        }

        first_node = GnodeToNode[cur_gnode][0];
        last_node = GnodeToNode[cur_gnode][GnodeToNode[cur_gnode].size() - 1];

        // if have previos node, we build a edge from previous node to 
        // currently the first node.
        if (prev_node_idx >= 0) {
            // printf("    InsertingB edge from %d\n", prev_node_idx);
            // printf("    InsertingB edge to %d - %p\n", NodePtrToIndex[last_node], last_node);
            if (edges[prev_node_idx].count(NodePtrToIndex[last_node]) == 0) {
                // debug_outputs.push_back(IndexToNodePtr[prev_node_idx]->debug_print() + " ===> " + last_node->debug_print());

                // printf("    Inserting B edge from \n");
                // std::cout << IndexToNodePtr[prev_node_idx]->debug_print() << "\n";
                // printf("    Inserting B edge to \n");
                // std::cout << last_node->debug_print() << "\n";
                edges[prev_node_idx].insert(NodePtrToIndex[last_node]);
                // printf("from %d to %d --- \n", prev_node_idx, NodePtrToIndex[last_node]);
            }
        }

    }
    else {
        // Ignore all untagged nodes
        return;
    }

    assert(first_node && last_node && "first_node and last_node are empty!");

    // Avoid duplicate building.
    if (visited_nodes.count(cur_gnode)) return;
    visited_nodes.insert(cur_gnode);

    // Iterate all sub nodes and build trees recursively
    if (auto *FN = dyn_cast<FunctionNode>(cur_gnode)) {
        size_t FN_idx = dgraph->FuncNodeToHash[FN];
        for (size_t sub_TN_idx : dgraph->FunctionNodeGraph_Typedep_Proto[FN_idx]) {
            auto *sub_node = dyn_cast<GeneralNode>(dgraph->TypeNodeList[sub_TN_idx]);
            // if (NodeToDCBs.count(cur_gnode))
            //     debug_outputs.push_back("from " + cur_gnode->debug_print() + " to " + sub_node->debug_print());
            std::vector<GeneralNode *>temp_vec = path;
            path.push_back(cur_gnode);
            recursiveBuilding(sub_node, NodePtrToIndex[first_node], NodeToDCBs, dgraph);
            path = temp_vec;
        }
        for (size_t sub_TN_idx : dgraph->FunctionNodeGraph_Typedep_Body[FN_idx]) {
            auto *sub_node = dyn_cast<GeneralNode>(dgraph->TypeNodeList[sub_TN_idx]);
            // if (NodeToDCBs.count(cur_gnode))
            //     debug_outputs.push_back("from " + cur_gnode->debug_print() + " to " + sub_node->debug_print());
            std::vector<GeneralNode *>temp_vec = path;
            path.push_back(cur_gnode);
            recursiveBuilding(sub_node, NodePtrToIndex[first_node], NodeToDCBs, dgraph);
            path = temp_vec;
        }
        for (size_t sub_GN_idx : dgraph->FunctionNodeGraph_GVdep[FN_idx]) {
            dgraph->GlobalVarNodeList[sub_GN_idx]->debug_print();
            auto *sub_node = dyn_cast<GeneralNode>(dgraph->GlobalVarNodeList[sub_GN_idx]);
            // if (NodeToDCBs.count(cur_gnode))
            //     debug_outputs.push_back("from " + cur_gnode->debug_print() + " to " + sub_node->debug_print());
            std::vector<GeneralNode *>temp_vec = path;
            path.push_back(cur_gnode);
            recursiveBuilding(sub_node, NodePtrToIndex[first_node], NodeToDCBs, dgraph);
            path = temp_vec;
        }
        for (size_t sub_FN_idx : dgraph->FunctionNodeGraph_Funcdep[FN_idx]) {
            auto *sub_node = dyn_cast<GeneralNode>(dgraph->FuncNodeList[sub_FN_idx]);
            // debug_outputs.push_back("from " + cur_gnode->debug_print() + " to " + sub_node->debug_print());
            std::vector<GeneralNode *>temp_vec = path;
            path.push_back(cur_gnode);
            recursiveBuilding(sub_node, NodePtrToIndex[first_node], NodeToDCBs, dgraph);
            path = temp_vec;
        }
    }
    
    else if (auto *TN = dyn_cast<TypeNode>(cur_gnode)) {
        size_t TN_idx = dgraph->TypeNodeToHash[TN];
        for (size_t sub_TN_idx : dgraph->TypeNodeGraph[TN_idx]) {
            auto *sub_node = dyn_cast<GeneralNode>(dgraph->TypeNodeList[sub_TN_idx]);
            // if (NodeToDCBs.count(cur_gnode))
            //     debug_outputs.push_back("from " + cur_gnode->debug_print() + " to " + sub_node->debug_print());
            std::vector<GeneralNode *>temp_vec = path;
            path.push_back(cur_gnode);
            recursiveBuilding(sub_node, NodePtrToIndex[first_node], NodeToDCBs, dgraph);
            path = temp_vec;
        }
    }

    else if (auto *GN = dyn_cast<GlobalVariableNode>(cur_gnode)) {
        size_t GN_idx = dgraph->GlobalVarNodeToHash[GN];
        for (size_t sub_TN_idx : dgraph->GlobalVarNodeGraph[GN_idx]) {
            auto *sub_node = dyn_cast<GeneralNode>(dgraph->TypeNodeList[sub_TN_idx]);
            // if (NodeToDCBs.count(cur_gnode))
            //     debug_outputs.push_back("from " + cur_gnode->debug_print() + " to " + sub_node->debug_print());
            std::vector<GeneralNode *>temp_vec = path;
            path.push_back(cur_gnode);
            recursiveBuilding(sub_node, NodePtrToIndex[first_node], NodeToDCBs, dgraph);
            path = temp_vec;
        }
    }

    else {
        // TODO
        // This should be unreachable
    }
}

void ShrinkedGraph::buildShrinkedGraph(DependencyGraph * dgraph, 
    std::unordered_map<GeneralNode *, std::unordered_set<DiffCodeBlock *> > &NodeToDCBs) {

    // printf("[DEBUG] get in buildShrinkedGraph\n");
    shrinked_node_num = 0;
    for (auto p : NodeToDCBs) {
        auto *gnode = p.first;
        recursiveBuilding(gnode, -1, NodeToDCBs, dgraph);
    }

    // for (int i = 0; i < shrinked_node_num; i++ ) {
    //     std::string output = "";
    //     output += "" + IndexToNodePtr[i]->debug_print() + "\n";
    //     output += "     " + IndexToNodePtr[i]->DCB->debug_print() + "\n";
    //     debug_outputs.push_back(output);
    // }
}

void ShrinkedGraph::nodeMatching(ShrinkedGraph * sgraph) {

    struct Key {
        size_t desc;
        DiffCodeBlock* dcb_ptr;

        bool operator==(const Key &other) const {
            return desc == other.desc && dcb_ptr == other.dcb_ptr;
        }
    };

    struct KeyHasher {
        std::size_t operator()(const Key& k) const {
            return k.desc ^ std::hash<DiffCodeBlock *>()(k.dcb_ptr);
        }
    };

    std::unordered_map<Key, Node *, KeyHasher> DescHashToNode_A;

    for (auto *node_a: nodes) {
        DescHashToNode_A[{node_a->gnode->getDescriptionHash(), node_a->DCB}] = node_a;
        // if (node_a->gnode->name == "drm_gem_object_free") {
            printf("[a] %s %llu %p %d\n", node_a->debug_print().c_str(), node_a->gnode->getDescriptionHash(), node_a->DCB, node_a->gnode->getKind());
        // }

    }
        

    for (auto *node_b : sgraph->nodes) {
        // if (node_b->gnode->name == "drm_gem_object_free") {
            printf("[b] %s %llu %p %d\n", node_b->debug_print().c_str(), node_b->gnode->getDescriptionHash(), node_b->DCB, node_b->gnode->getKind());
        // }
        Key p = {node_b->gnode->getDescriptionHash(), node_b->DCB->getMirror()};
        if (DescHashToNode_A.count(p)) {

            auto *node_a = DescHashToNode_A[p];
            printf("    [matching found!] %s %llu %p %d\n", node_a->debug_print().c_str(), node_a->gnode->getDescriptionHash(), node_a->DCB, node_a->gnode->getKind());

            if (!matching_nodes.count(node_a) && !matching_nodes.count(node_b)) {
                // Should never match one node multiple times, however, it may 
                // not the case because of C++ non-mangled names.
                // Thus we compromise to match all the nodes.
                matching_nodes[node_a] = node_b;
                matching_nodes[node_b] = node_a;
            }
            else {
                // Should never reach here.
                // But currently we just ignore and not regard it as incorrectness.
            }
        }
    }
}

bool ShrinkedGraph::doviolatedAppliedCBDetect(Node *node_a, Node *node_b, ShrinkedGraph * sgraph, std::unordered_set<Node *> &vis) {
    if ( (node_a && vis.count(node_a)) || 
            (node_b && vis.count(node_b)) ) return false;

    if (node_a)
        vis.insert(node_a);      
    if (node_b)
        vis.insert(node_b);

    Node *cur_node = nullptr;
    unsigned cur_idx = -1;
    DiffCodeBlock *cur_DCB = nullptr;
    bool is_applied_VA = true;
    bool is_conflict = false;

    DiffCodeBlock *DCB_A = nullptr;
    DiffCodeBlock *DCB_B = nullptr;
    int idx_b = -1;
    int idx_a = -1;

    if (node_a) {
        DCB_A = node_a->DCB;
        idx_a = NodePtrToIndex[node_a];
    }

    if (node_b) {
        DCB_B = node_b->DCB;
        idx_b = sgraph->NodePtrToIndex[node_b];
    } 

    cur_node = node_a;
    cur_DCB = DCB_A;
    if ( (DCB_A && DCB_A->getDiffType() == APPLYB) || 
            (DCB_B && DCB_B->getDiffType() == APPLYB) ) {
        cur_DCB = DCB_B;
        cur_node = node_b;
        is_applied_VA = false;
    }
    else if ((DCB_A && DCB_A->getDiffType() == CONFLICT) || 
            (DCB_B && DCB_B->getDiffType() == CONFLICT) )
        is_conflict = true;

    // current level encounters CONFLICT
    if (is_conflict) {
        // 1. mark all CONFLICT related node unremovable
        // 2. mark all nodes connected to CONFLICT related node unremovable
        printf("\n[Current Conflict]\n");
        if (node_a) {
            printf("    |\n");
            printf("     ----- [Current A] ");
            std::cout << node_a->debug_print() << std::endl;
            unremovable_nodes.insert(node_a);
            for (auto p_idx : edges[idx_a]) {
                Node *p_node_a = IndexToNodePtr[p_idx];
                DiffCodeBlock *p_DCB = p_node_a->DCB; 
                 // parent level applied differently from current level
                if (p_DCB->getDiffType() == APPLYB ) {
                    if (matching_nodes.count(p_node_a) == 0) {
                        unremovable_nodes.insert(p_node_a);
                        printf("        |\n");
                        printf("        ----- [Parent B (applied)] ");
                        std::cout << p_node_a->debug_print() << std::endl;                         
                    }
                }
                else if (p_DCB->getDiffType() == CONFLICT) {
                    unremovable_nodes.insert(p_node_a);
                    printf("        |\n");
                    printf("        ----- [Conflict Parent A] ");
                    std::cout << p_node_a->debug_print() << std::endl;

                    if (matching_nodes.count(p_node_a) != 0) {
                        unremovable_nodes.insert(matching_nodes[p_node_a]);
                        printf("        |\n");
                        printf("        ----- [Conflict Parent B] ");
                        std::cout << matching_nodes[p_node_a]->debug_print() << std::endl;
                    }
                }
            }   
        }
        if (node_b) {
            printf("    |\n");
            printf("     ----- [Current B] ");
            std::cout << node_b->debug_print() << std::endl;
            unremovable_nodes.insert(node_b);
            for (auto p_idx : node_b->sgraph->edges[idx_b]) {
                Node *p_node_b = node_b->sgraph->IndexToNodePtr[p_idx];
                DiffCodeBlock *p_DCB = p_node_b->DCB; 
                // parent level applied differently from current level
                if (p_DCB->getDiffType() == APPLYA ) {
                    if (matching_nodes.count(p_node_b) == 0) {
                        unremovable_nodes.insert(p_node_b);
                        printf("        |\n");
                        printf("        ----- [Parent A (applied)] ");
                        std::cout << p_node_b->debug_print() << std::endl;                         
                    }
                }
                else if (p_DCB->getDiffType() == CONFLICT) {
                    unremovable_nodes.insert(p_node_b);
                    printf("        |\n");
                    printf("        ----- [Conflict Parent B] ");
                    std::cout << p_node_b->debug_print() << std::endl;

                    if (matching_nodes.count(p_node_b) != 0) {
                        unremovable_nodes.insert(matching_nodes[p_node_b]);
                        printf("        |\n");
                        printf("        ----- [Conflict Parent A] ");
                        std::cout << matching_nodes[p_node_b]->debug_print() << std::endl;
                    }
                }
            }
        }   
    }

    // current level applied either of the variant.
    else {
        // if current applied node is nullptr, it has no dependency node
        if (!cur_node) return false;
        // printf(" ====== cur_node ======= \n");
        // cur_node->debug_print();
        auto &cur_edges = is_applied_VA ? 
                    edges : node_b->sgraph->edges;
        auto &cur_indexToNode = is_applied_VA ? 
                    IndexToNodePtr : node_b->sgraph->IndexToNodePtr;
        auto &cur_NodeToIndex = is_applied_VA ?
                    NodePtrToIndex : node_b->sgraph->NodePtrToIndex;
        bool printed = false;
        for (auto p_idx : cur_edges[cur_NodeToIndex[cur_node]]) {
            Node *p_node = cur_indexToNode[p_idx];
            DiffCodeBlock *p_DCB = p_node->DCB; 
            // errs() << "~~~~~~~~~~~~~~~~~~~~~~~~~\n";
            // errs() << cur_node->start_line << " - " << cur_node->end_line << ": " << cur_node->gnode->debug_print() << "\n";
            // errs() << p_node->start_line << " - " << p_node->end_line << ": " << p_node->gnode->debug_print() << "\n";
            // errs() << "~~~~~~~~~~~~~~~~~~~~~~~~~\n";

            // parent level applied differently from current level
            if ((p_DCB->getDiffType() == APPLYA && !is_applied_VA) || 
                    (p_DCB->getDiffType() == APPLYB && is_applied_VA)) {
                
                if (matching_nodes.count(p_node) == 0) {
                    // if no matching node found, all current level and parent level
                    // nodes should be unremovable.
                    if (!printed) {
                        printf("\n[Current Applied Different with Parent]\n");
                        printed = true;
                    }
                    if (node_a) {
                        unremovable_nodes.insert(node_a);
                        printf("    |\n");
                        printf("     ----- [Current A%s] ", is_applied_VA ? " (applied)" : "");
                        std::cout << node_a->debug_print() << std::endl;
                    }
                        
                    if (node_b) {
                        unremovable_nodes.insert(node_b);
                        printf("    |\n");
                        printf("     ----- [Current B%s] ", !is_applied_VA ? " (applied)" : "");
                        std::cout << node_b->debug_print() << std::endl;
                    }
                    unremovable_nodes.insert(p_node);
                    printf("        |\n");
                    printf("        ----- [Parent %c] ", p_DCB->isFromVariantA() ? 'A' : 'B');
                    std::cout << p_node->debug_print() << std::endl << std::endl;
                    printed = true;
                    // assert(path_along.count(idx_a << 15 | p_idx));
                    // for (auto *gn :  path_along[idx_a << 15 | p_idx])
                    //     std::cout << "      ->" << gn->debug_print() << std::endl;
                    
                        
                }
            }
            else if (p_DCB->getDiffType() == CONFLICT) {
                // parent level reach CONFLICT
                // As parents are conflict, they must be unremovable
                // unremovable_nodes.insert(p_node);

                // printf(" <<<<< parent matching node inserting >>>>  \n");
                // std::cout << p_node->debug_print() << std::endl;

                if (matching_nodes.count(p_node) != 0) {
                    // unremovable_nodes.insert(matching_nodes[p_node]);
                    // printf(" <<<<< parent matching node inserting >>>> \n");
                    // std::cout << matching_nodes[p_node]->debug_print() << std::endl;
                }

                // if parent cannot find matching point, mark current level as unremovable
                else {
                    // printf("Insert unremovable_nodes for (3)\n");
                    if (node_a) {
                        if (!printed) {
                            printf("\n[Current Applied But Parent Conflict]\n");
                            printed = true;
                        }
                        unremovable_nodes.insert(node_a);
                        printf("    |\n");
                        printf("     ----- [Current A%s] ", is_applied_VA ? " (applied)" : "");
                        std::cout << node_a->debug_print() << std::endl;
                    }
                        
                    if (node_b) {
                        if (!printed) {
                            printf("\n[Current Applied But Parent Conflict]\n");
                            printed = true;
                        }
                        unremovable_nodes.insert(node_b);
                        printf("    |\n");
                        printf("     ----- [Current B%s] ", !is_applied_VA ? " (applied)" : "");
                        std::cout << node_b->debug_print() << std::endl;
                    }

                    printf("        |\n");
                    printf("        ----- [Conflict Parent %c] ", p_DCB->isFromVariantA() ? 'A' : 'B');
                    std::cout << p_node->debug_print() << std::endl;

                    if (matching_nodes.count(p_node) != 0) {
                        printf("        |\n");
                        printf("        ----- [Conflict Parent %c] ", matching_nodes[p_node]->DCB->isFromVariantA() ? 'A' : 'B');
                        std::cout << matching_nodes[p_node]->debug_print() << std::endl;
                    }
                }
            }
        }
    }
    return true;
}

void ShrinkedGraph::violatedAppliedCBDetect(ShrinkedGraph * sgraph) {
    std::unordered_set<Node *> vis;
    for (auto *node : nodes) {
        // printf("node from va ==> %p\n", node);
        if (vis.count(node) == 0) {
            Node *node_another = matching_nodes.count(node) ? matching_nodes[node] : nullptr;
            doviolatedAppliedCBDetect(node, node_another, sgraph, vis);
        }
    }

    for (auto *node : sgraph->nodes) {
        // printf("node from vb ==> %p\n", node);
        if (vis.count(node) == 0) {
            Node *node_another = matching_nodes.count(node) ? matching_nodes[node] : nullptr;
            doviolatedAppliedCBDetect(node_another, node, sgraph, vis);
        }
    }
}

/*
    TODO
    Fix when applied DCBs should be unremovable.
    At time, it should be multi-stated as well.
*/
void ShrinkedGraph::doGraphMerging(Node *node_a, Node *node_b, ShrinkedGraph * sgraph, 
                            int pre_idx, std::unordered_set<Node *> &vis) {
    MultiStatNode * cur_MSN = nullptr;
    Node *cur_node = nullptr;
    int cur_idx = -1;
    DiffCodeBlock *cur_DCB = nullptr;
    bool is_applied_VA = true;
    bool is_conflict = false;

    DiffCodeBlock *DCB_A = nullptr;
    DiffCodeBlock *DCB_B = nullptr;
    int idx_b = -1;
    int idx_a = -1;

    if (node_a) {
        DCB_A = node_a->DCB;
        idx_a = NodePtrToIndex[node_a];
    }

    if (node_b) {
        DCB_B = node_b->DCB;
        idx_b = sgraph->NodePtrToIndex[node_b];
    } 

    cur_node = node_a;
    cur_DCB = DCB_A;

    // printf("%p, ", node_a);
    // if (node_a)
    //     std::cout << node_a->debug_print() << std::endl;
    // else 
    //     std::cout << std::endl;
    // printf("%p, ", node_b);
    // if (node_b)
    //     std::cout << node_b->debug_print() << std::endl;
    // else 
    //     std::cout << std::endl;
    // printf("%d, %d\n", node_a ? unremovable_nodes.count(node_a) : -1, node_b ? unremovable_nodes.count(node_b) : -1);
    // printf("======================================\n");

    if ((node_a && unremovable_nodes.count(node_a)) || 
            (node_b && unremovable_nodes.count(node_b))) {
        is_conflict = true;
    }
    else if ( (DCB_A && DCB_A->getDiffType() == APPLYB) || 
            (DCB_B && DCB_B->getDiffType() == APPLYB) ) {
        cur_DCB = DCB_B;
        cur_node = node_b;
        is_applied_VA = false;
    }


    // if the current node is null, skip
    if (!is_conflict && !cur_node)
        return;

    // if the multi-state node has been created, use it directly
    if (is_conflict) {
        if (node_a && node_b && NodeToMSN.count(node_a))
            cur_MSN = NodeToMSN[node_a];
        if (node_a && !node_b && NodeToMSN.count(node_a))
            cur_MSN = NodeToMSN[node_a];
        if (!node_a && node_b && NodeToMSN.count(node_b))
            cur_MSN = NodeToMSN[node_b];
    }
    else {
        if (NodeToMSN.count(cur_node))
            cur_MSN = NodeToMSN[cur_node];
    }

    // otherwise, create new multi-state node for it
    if (!cur_MSN) {
        if (is_conflict) {
            if (node_a && node_b) {
                cur_MSN = new MultiStatNode(node_a, node_b);
                NodeToMSN[node_a] = NodeToMSN[node_b] = cur_MSN;
            }
                
            if (node_a && !node_b) {
                cur_MSN = new MultiStatNode(node_a, true);
                NodeToMSN[node_a] = cur_MSN;
            }
                
            if (!node_a && node_b) {
                cur_MSN = new MultiStatNode(node_b, true);
                NodeToMSN[node_b] = cur_MSN;
            }
        }
        else {
            cur_MSN = new MultiStatNode(cur_node);
            NodeToMSN[cur_node] = cur_MSN;
        }

        MSNToIndex[cur_MSN] = multi_stat_node_num;
        IndexToMSN[multi_stat_node_num] = cur_MSN;
        multi_stat_node_num++;

        // std::string output = "";
        // output += "[Current Node of u]" + cur_MSN->current_node->debug_print() + "\n";
        // if (cur_MSN->mirror_node)
        //     output += "     [Mirror Node of u]" + cur_MSN->current_node->debug_print() + "\n";
        // debug_outputs.push_back(output);

        if (multi_stat_node_num >= merged_edges.size())
            merged_edges.resize((int)(merged_edges.size() * 1.6 + 1));
    }

    cur_idx = MSNToIndex[cur_MSN];
    
    if (pre_idx >= 0) {
        // std::string output = "";
        // output += "Inserting        from: " + IndexToMSN[pre_idx]->current_node->debug_print();
        // output += "         to: " + IndexToMSN[cur_idx]->current_node->debug_print();
        // debug_outputs.push_back(output);
        merged_edges[pre_idx].insert(cur_idx);
    }

    if ( (node_a && vis.count(node_a)) || 
            (node_b && vis.count(node_b)) ) return;

    if (node_a)
        vis.insert(node_a);
    if (node_b)
        vis.insert(node_b);

    // if it's conflict or unremovable node, we consider the both parents
    if (is_conflict) {
        
        // For VA 
        if (node_a)
            for (auto next_idx : edges[NodePtrToIndex[node_a]]) {
                Node *next_node = IndexToNodePtr[next_idx];
                Node *next_node_a = nullptr;
                Node *next_node_b = nullptr;
                if (next_node->DCB->isFromVariantA()) {
                    next_node_a = next_node;
                    if (matching_nodes.count(next_node_a))
                        next_node_b = matching_nodes[next_node_a];
                }
                else {
                    next_node_b = next_node;
                    if (matching_nodes.count(next_node_b))
                        next_node_a = matching_nodes[next_node_b];
                }
                    
                doGraphMerging(next_node_a, next_node_b, sgraph, cur_idx, vis);
            }

        // For VB
        if (node_b)
            for (auto next_idx : sgraph->edges[sgraph->NodePtrToIndex[node_b]]) {
                Node *next_node = sgraph->IndexToNodePtr[next_idx];
                Node *next_node_a = nullptr;
                Node *next_node_b = nullptr;
                if (next_node->DCB->isFromVariantA()) {
                    next_node_a = next_node;
                    if (matching_nodes.count(next_node_a))
                        next_node_b = matching_nodes[next_node_a];
                }
                else {
                    next_node_b = next_node;
                    if (matching_nodes.count(next_node_b))
                        next_node_a = matching_nodes[next_node_b];
                }
                    
                doGraphMerging(next_node_a, next_node_b, sgraph, cur_idx, vis);
            }
    }

    else {
        auto &cur_edges = is_applied_VA ? 
                    edges : sgraph->edges;
        auto &cur_indexToNode = is_applied_VA ? 
                    IndexToNodePtr : sgraph->IndexToNodePtr;
        auto &cur_NodeToIndex = is_applied_VA ?
                    NodePtrToIndex : sgraph->NodePtrToIndex;
        for (auto next_idx : cur_edges[cur_NodeToIndex[cur_node]]) {
            Node *next_node = cur_indexToNode[next_idx];
            Node *next_node_a = nullptr;
            Node *next_node_b = nullptr;
            if (next_node->DCB->isFromVariantA()) {
                next_node_a = next_node;
                if (matching_nodes.count(next_node_a))
                    next_node_b = matching_nodes[next_node_a];
            }
            else {
                next_node_b = next_node;
                if (matching_nodes.count(next_node_b))
                    next_node_a = matching_nodes[next_node_b];
            }
            doGraphMerging(next_node_a, next_node_b, sgraph, cur_idx, vis);
        }
    }
    
}

void ShrinkedGraph::graphMerging(ShrinkedGraph * sgraph) {
    multi_stat_node_num = 0;
    merged_edges.resize(edges.size());
    std::unordered_set<Node *> vis;
    for (auto *node : nodes) {
        if (vis.count(node) == 0) {
            Node *node_another = matching_nodes.count(node) ? matching_nodes[node] : nullptr;
            doGraphMerging(node, node_another, sgraph, -1, vis);
        }
    }

    for (auto *node : sgraph->nodes) {
        if (vis.count(node) == 0) {
            Node *node_another = matching_nodes.count(node) ? matching_nodes[node] : nullptr;
            doGraphMerging(node_another, node, sgraph, -1, vis);
        }
    }
}

void ShrinkedGraph::tarjan(int v) {
    dfn[v] = low[v] = ++timestamp;
    tarjan_stack.push(v);
    in_tarjan_stack[v] = true;

    for (auto u : merged_edges[v]) {
        if (!dfn[u]) {
            tarjan(u);
            low[v] = std::min(low[v], low[u]);
        }
        else if (in_tarjan_stack[u]) {
            low[v] = std::min(low[v], low[u]);
        }
    }

    if (dfn[v] == low[v]) {
        scc_num++;
        int u;
        do {
            u = tarjan_stack.top();
            in_tarjan_stack[u] = false;
            tarjan_stack.pop();
            color[u] = scc_num;
            node_groups[scc_num].insert(u);


        } while (v != u);
    }
}

void ShrinkedGraph::eliminateCycle() {
    scc_num = 0;
    dfn.resize(multi_stat_node_num + 5, 0);
    low.resize(multi_stat_node_num + 5, 0);
    color.resize(multi_stat_node_num + 5, 0);
    node_groups.resize(multi_stat_node_num + 5);
    in_tarjan_stack.resize(multi_stat_node_num + 5, false);
    timestamp = 0;
    
    // do cycle shrinking
    for (int i = 0; i < multi_stat_node_num; i++) {
        if (!dfn[i]) tarjan(i); 
    }

    // rebuild shrinked acyclic graph
    shrinked_edges.resize(scc_num + 5);
    shrinked_directed_edges.resize(scc_num + 5);
    for (int i = 0; i < multi_stat_node_num; i++) {
        for (auto j : merged_edges[i]) {
            if (color[i] != color[j]) {
                // build graph with direction
                // printf("Build shrinked_directed_edges from %d (%d item) to %d (%d item)\n", color[i], node_groups[color[i]].size(), color[j], node_groups[color[j]].size());
                shrinked_directed_edges[color[i]].insert(color[j]);

                // build graph without direction
                shrinked_edges[color[i]].insert(color[j]);
                shrinked_edges[color[j]].insert(color[i]);
            }
        }
    }

    // for (int i = 1; i <= scc_num; i++) {
    //     std::string output = "==================================\n";
    //     for (auto u : node_groups[i]) {
    //         auto *msn_u = IndexToMSN[u];
    //         output += "[Current Node of u]" + msn_u->current_node->debug_print() + "\n";
    //         if (msn_u->mirror_node)
    //             output += "     [Mirror Node of u]" + msn_u->mirror_node->debug_print() + "\n";
    //         debug_outputs.push_back(output);
    //     }
    // }
}

void ShrinkedGraph::doClassifyGraph(int u) {
    if (visited[u]) return;

    // std::string output = "==================================\n";
    // for (auto v : node_groups[u]) {
    //     auto *msn_u = IndexToMSN[v];
    //     output += "[Current Node of u]" + msn_u->current_node->debug_print() + "\n";
    //     if (msn_u->mirror_node)
    //         output += "     [Mirror Node of u]" + msn_u->mirror_node->debug_print() + "\n";
    //     debug_outputs.push_back(output);
    // }
    visited[u] = true;

    classified_nodes[class_num].insert(u);
    for (auto v : shrinked_edges[u]) {
        doClassifyGraph(v);
    }
}

void ShrinkedGraph::classifyGraph() {
    visited.resize(scc_num + 5, false);
    classified_nodes.resize(scc_num + 5);
    class_num = 0;

    for (int i = 1; i <= scc_num; i++)
        if (!visited[i]) {
            class_num++;
            doClassifyGraph(i);
        }     

    // for (int i = 1; i <= class_num; i++) {
    //     for (int t : classified_nodes[i]) {
    //         for (auto v : node_groups[t]) {
    //             auto *msn_u = IndexToMSN[v];
    //             std::string output = "";
    //             output += "[---Current Node of u]" + msn_u->current_node->debug_print() + "\n";
    //             if (msn_u->mirror_node)
    //                 output += "     [---Mirror Node of u]" + msn_u->mirror_node->debug_print() + "\n";
    //             debug_outputs.push_back(output);
    //         }
    //     }
    // }
}

void ShrinkedGraph::doTopologySort(int class_id) {
    int level = 0;
    std::queue<int> q;


    for (auto u : classified_nodes[class_id]) {
        if (inDegree[u] == 0) {
            q.push(u);
            top_order[u] = level;
        }
    }
    level++;
    while ((!q.empty())) {
        int u = q.front();
        q.pop();

        // for (auto v : node_groups[u]) {
        //     auto *msn_u = IndexToMSN[v];
        //     std::string output = "";
        //     output += "[---Current Node of u]" + msn_u->current_node->debug_print() + "\n";
        //     if (msn_u->mirror_node)
        //         output += "     [---Mirror Node of u]" + msn_u->mirror_node->debug_print() + "\n";
        //     debug_outputs.push_back(output);
        // }

        // The later item should be handled first further.
        // printf("[TOPSORT] class %d push %d\n", class_id, u);
        topsort_results[class_id].push_back(u);
        // top_order[class_id] = num;
        
        for (auto v : shrinked_directed_edges[u]) {
            inDegree[v]--;
            if (inDegree[v] == 0) {
                q.push(v);
                top_order[v] = level;
            }
        }
        level++;
    }
}

void ShrinkedGraph::topologySort() {
    inDegree.resize(scc_num + 5, 0);
    topsort_results.resize(scc_num + 5);
    top_order.resize(scc_num + 5);
    for (auto s : shrinked_directed_edges) {
        for (auto v : s) {
            inDegree[v]++;
        }
    }

    for (int i = 1; i <= class_num; i++) {
        doTopologySort(i);
    }
}

void ShrinkedGraph::showResults(std::string output_path) {
    std::ofstream out(output_path);
    if (!out.is_open()) {
        fprintf(stderr, "Cannot open output file...\n");
        exit(-1);
    } 
    std::unordered_set<Node *> printed_nodes;
    std::unordered_set<DiffCodeBlock *> calculated_in_ELoc;

    int group_num = 0;

    // Vars for evaluation
    // ELoC 
    int extra_code_VA = 0;
    int extra_code_VB = 0;
    if (!class_num) {
        out << "xxx\n";
        return;
    }

    for (int i = 1; i <= class_num; i++) {
        std::string group_info = "[output] Group " + std::to_string(group_num) + "\n";
        bool print_info_group = true;
        for (int j = topsort_results[i].size() - 1; j >= 0; j--) {
            auto scc = topsort_results[i][j];
            std::string scc_info = "    [output] Rank -> " + std::to_string(top_order[scc]) + 
                 ", Scc has " + std::to_string(node_groups[scc].size()) + " items\n";
            bool print_info_scc = true;
            for (auto u : node_groups[scc]) {
                if (unremovable_nodes.count(IndexToMSN[u]->current_node)) {
                    if (print_info_group) {
                        out << group_info;
                        group_num++;
                        print_info_group = false;
                    }
                    if (print_info_scc) {
                        out << scc_info;
                        print_info_scc = false;
                    }
                    DiffCodeBlock *DCB_A, *DCB_B;
                    Node *current_node = IndexToMSN[u]->current_node;
                    std::string type;
                    if (current_node->DCB->getDiffType() == APPLYA) type = "Apply A";
                    else if (current_node->DCB->getDiffType() == APPLYB) type = "Apply B";
                    else type = "Conflict";

                    if (current_node->DCB->isFromVariantA())
                        DCB_A = current_node->DCB, DCB_B = current_node->DCB->getMirror();
                    else 
                        DCB_B = current_node->DCB, DCB_A = current_node->DCB->getMirror();

                    // Recording for evaluation
                    // For ELoC
                    if (current_node->DCB->getDiffType() != CONFLICT) {
                        if (!calculated_in_ELoc.count(DCB_A)) {
                            extra_code_VA += DCB_A->getEndLine() - DCB_A->getStartLine() + 1;
                            calculated_in_ELoc.insert(DCB_A);
                        }
                        if (!calculated_in_ELoc.count(DCB_B)) {
                            extra_code_VB += DCB_B->getEndLine() - DCB_B->getStartLine() + 1;
                            calculated_in_ELoc.insert(DCB_B);
                        }
                    }

                    // Conjugate
                    if (IndexToMSN[u]->mirror_node) {
                        Node *mirror_node = IndexToMSN[u]->mirror_node;
                        if (!printed_nodes.count(mirror_node) && !printed_nodes.count(current_node)) {
                            if (current_node->DCB->isFromVariantA())
                                out << "        [output] CB Type: " << type << ", filepath = " << current_node->DCB->getSourceFileName()
                                    << ", change(" << current_node->start_line << ", " << current_node->end_line
                                    << ") in DCB_A(" << DCB_A->getStartLine() << ", " << DCB_A->getEndLine()
                                    << ") >>>>> change(" << mirror_node->start_line << ", " << mirror_node->end_line
                                    << ") in DCB_B(" << DCB_B->getStartLine() << ", " << DCB_B->getEndLine() << ")\n";
                            else 
                                out << "        [output] CB Type: " << type << ", filepath = " << current_node->DCB->getSourceFileName()
                                    << ", change(" << mirror_node->start_line << ", " << mirror_node->end_line
                                    << ") in DCB_A(" << DCB_A->getStartLine() << ", " << DCB_A->getEndLine()
                                    << ") >>>>> change(" << current_node->start_line << ", " << current_node->end_line
                                    << ") in DCB_B(" << DCB_B->getStartLine() << ", " << DCB_B->getEndLine() << ")\n";

                            printed_nodes.insert(current_node);
                            printed_nodes.insert(mirror_node);
                        }
                    }

                    if (current_node->DCB->isFromVariantA() && !printed_nodes.count(current_node)) {
                        out << "        [output] CB Type: " << type << ", filepath = " << current_node->DCB->getSourceFileName()
                            << ", change(" << current_node->start_line << ", " << current_node->end_line
                            << ")  in DCB_A(" << DCB_A->getStartLine() << ", " << DCB_A->getEndLine()
                            << ") >>>>> DCB_B(" << DCB_B->getStartLine() << ", " << DCB_B->getEndLine() << ")\n";
                        printed_nodes.insert(current_node);
                    }

                    if (!current_node->DCB->isFromVariantA() && !printed_nodes.count(current_node)) {
                        out << "        [output] CB Type: " << type << " filepath = " << current_node->DCB->getSourceFileName()
                            << ", DCB_A(" << DCB_A->getStartLine() << ", " << DCB_A->getEndLine()
                            << ") >>>>> change(" << current_node->start_line << ", " << current_node->end_line
                            << ") in DCB_B(" << DCB_B->getStartLine() << ", " << DCB_B->getEndLine() << ")\n";
                        printed_nodes.insert(current_node);
                    }
                }
            }
        }
    }

    // Print for evaluation
    // Print for total ELoC
    printf("ELoC: VA = %d, VB = %d\n", extra_code_VA, extra_code_VB);
}

void ShrinkedGraph::emitSuggestions(std::string output_path) {
    eliminateCycle();
    classifyGraph();
    topologySort();
    showResults(output_path);
}
     