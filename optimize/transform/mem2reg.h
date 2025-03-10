#ifndef MEM2REG_H
#define MEM2REG_H
#include "../../include/ir.h"
#include "../pass.h"

#include "../analysis/dominator_tree.h"
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class Mem2RegPass : public IRPass {
private:
    DomAnalysis *domtrees;
    // TODO():添加更多你需要的成员变量
    void IsPromotable(CFG *C, Instruction AllocaInst);
    void Mem2RegNoUseAlloca(CFG *C, std::set<int> &vset);

    void Mem2RegUseDefInSameBlock(CFG *C, std::set<int> &vset, int block_id);
    void Mem2RegOneDefDomAllUses(CFG *C, std::set<int> &vset);
    void InsertPhi(CFG *C);
    void VarRename(CFG *C);
    void Mem2Reg(CFG *C);

    // 添加的成员函数
    void FindDefs(CFG *c);
    void FindDefsAndUses_DCE(CFG *c);
    // key:变量的声明寄存器  value:对该变量定义的 label 集合
    std::map<int, std::set<int>> defs_id;

    // key: var声明时的寄存器, value: label中最后对var赋值的寄存器
    std::map<int, std::map<int, int>> var2defs;
    // key: 变量, value: 使用该变量的指令集合
    std::map<Operand, std::set<Instruction>> varUsed;
    // key: 变量, value: 定义该变量的指令(pair.first: label, pair.second: 块内的指令位置)
    std::map<Operand, std::vector<std::pair<int, int>>> varDefines;
    // 变量集合
    std::set<Operand> vars;

public:
    Mem2RegPass(LLVMIR *IR, DomAnalysis *dom) : IRPass(IR) { domtrees = dom; }
    void Execute();
    void DCE_Execute();
    void DCE(CFG *c);
};

#endif