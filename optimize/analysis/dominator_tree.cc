#include "dominator_tree.h"
#include "../../include/ir.h"
#include <algorithm>
#include <set>
#include <vector>

void DomAnalysis::Execute() {
    for (auto [defI, cfg] : llvmIR->llvm_cfg) {
        DomInfo[cfg].C = llvmIR->llvm_cfg[defI];
        DomInfo[cfg].BuildDominatorTree();
    }
}
void DominatorTree::BuildTree() {
    idom.clear();
    dom_tree.clear();
    dom_relation.clear();
    idom.resize(C->max_label + 1, nullptr);
    dom_tree.resize(C->max_label + 1);
    // dom_relation[i] 存储支配 i 的所有节点
    dom_relation.resize(C->max_label + 1);

    // 建立支配关系: dom[i][j] = 1 表示 i 被 j 支配
    /*
    迭代数据流:
        初始化 dom 数组:
        第 0 行: dom[0][0] = 1, 其他全为 0
        其他   : 全为 1
    */
    dom.clear();
    dom.resize(C->max_label + 1);
    for (int v : C->rev_ord) {
        dom[v] = std::vector<bool>(C->max_label + 1, true);
    }
    dom[0] = std::vector<bool>(C->max_label + 1, false);
    dom[0][0] = true;
    bool flag = true;
    auto ord = C->rev_ord;
    std::reverse(ord.begin(), ord.end());
    // 将每个结点上的支配点集不断迭代直至答案不变即可
    while (flag) {
        flag = false;
        for (int v : ord) {
            if (v == 0) {
                continue;
            }
            auto tmp = dom[v];
            // 一个点的支配点的点集为它所有前驱结点的支配点集的交集，再并上它本身
            auto pres = C->GetPredecessor(v);
            for (auto p : pres) {
                for (int i = 0; i <= C->max_label; i++) {
                    // 更新当前的支配关系
                    tmp[i] = tmp[i] && dom[p->block_id][i];
                }
            }
            tmp[v] = true;

            if (dom[v] != tmp) {
                // 支配关系发生变化，继续迭代
                flag = true;
            }
            dom[v] = tmp;
        }
    }
    for (int i = 1; i <= C->max_label; i++) {
        for (int j = 0; j <= C->max_label; j++) {
            if (dom[i][j]) {
                // i 的所有支配节点
                dom_relation[i].push_back(j);
                // std::cout << i << "<-" << j << "\n";
            }
        }
    }

    // std::cout << "pver\n";

    // 计算直接支配者
    idom[0] = (*C->block_map)[0];
    for (int u = 1; u <= C->max_label; u++) {
        int imm = -1;
        for (int v : dom_relation[u]) {
            if (v == u)
                continue;
            if (imm == -1) {
                imm = v;
            } else if (dom_relation[imm].end() == std::find(dom_relation[imm].begin(), dom_relation[imm].end(), v)) {
                // 如果 v 被 imm 支配: imm -> .. -> v -> .. -> u, 更新 imm
                imm = v;
            }
        }

        // std::cout << imm << "\n";
        idom[u] = (*C->block_map)[imm];
        // std::cout << idom[u]->block_id << " -> " << u << "\n";
    }
}
void DominatorTree::BuildDF() {
    /*
    支配边界定义:
        1. n 支配 m 的一个前驱节点, q ∈ preds(m) && n ∈ Dom(q)
        2. n 不直接支配 m, 即 n = m 或 n ∉ Dom(m)
        则 m 是 n 的一个支配边界

    两种情况: 循环汇合点和条件分支汇合点
        1.  b1 -> b2 -> b3 -> ..
                  ^-----|
            b3 是 b2 的前驱节点, 且 b2 支配 b3, 因此 b2 支配 b2 的前驱 b3
            且 b2 = b2, b2 是 b2 的一个支配边界

        2.  b1 -> b3 -> ..
            b2 ---^
            b1 是 b3 的前驱, 且 b1 支配 b1, 因此 b3 是 b1 的一个支配边界

    Reference: https://www.zhihu.com/question/293748348
    */
    // dom_frontier[i]: i 的支配边界有哪些
    dom_frontier.clear();
    dom_frontier.resize(C->max_label + 1);

    for (int n : C->rev_ord) {
        auto pres = C->GetPredecessor(n);
        if (pres.size() == 0) {
            continue;
        }
        int run = -1;
        for (auto pp : pres) {
            // 对于 n 的前驱节点 p, 满足条件 1
            int p = pp->block_id;
            run = p;
            // 沿着 idom 关系向上遍历, 直至遇到 n 的直接支配节点, 满足条件 2
            while (run != idom[n]->block_id) {
                // 添加所有遍历过的节点
                dom_frontier[run].insert(n);
                run = idom[run]->block_id;
            }
        }
    }
}

void DominatorTree::BuildDominatorTree(bool reverse) {
    // TODO("BuildDominatorTree");
    if (reverse) {
        auto tmp = C->G;
        C->G = C->invG;
        C->invG = tmp;
    }
    BuildTree();
    BuildDF();
}

std::set<int> DominatorTree::GetDF(std::set<int> S) {
    // TODO("GetDF");
    std::set<int> res;
    for (auto s : S) {
        res.merge(dom_frontier[s]);
    }
    return res;
}

std::set<int> DominatorTree::GetDF(int id) {
    // TODO("GetDF");
    return dom_frontier[id];
}

bool DominatorTree::IsDominate(int id1, int id2) {
    // TODO("IsDominate");
    return dom[id2][id1];
}
