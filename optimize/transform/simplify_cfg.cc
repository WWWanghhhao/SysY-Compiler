#include "simplify_cfg.h"
#include <deque>
#include <stack>
#include <vector>

void SimplifyCFGPass::Execute() {
    for (auto [defI, cfg] : llvmIR->llvm_cfg) {
        EliminateUnreachedBlocksInsts(cfg);
    }
}

// 删除从函数入口开始到达不了的基本块和指令
// 不需要考虑控制流为常量的情况，你只需要从函数入口基本块(0号基本块)开始dfs，将没有被访问到的基本块和指令删去即可
void SimplifyCFGPass::EliminateUnreachedBlocksInsts(CFG *c) {
    // TODO("EliminateUnreachedBlocksInsts");
    // 在建立 CFG 的时候已经实现该功能
}