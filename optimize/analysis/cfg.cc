#include "../../include/Instruction.h"
#include "../../include/ir.h"
#include <assert.h>
#include <deque>
#include <map>
#include <stack>
#include <vector>

void LLVMIR::CFGInit() {
    for (auto &[defI, bb_map] : function_block_map) {
        CFG *cfg = new CFG();
        cfg->block_map = &bb_map;
        cfg->function_def = defI;
        cfg->BuildCFG();
        // TODO("init your members in class CFG if you need");
        llvm_cfg[defI] = cfg;
        cfg->max_reg = def_reg[defI];
    }
}

void LLVMIR::BuildCFG() {
    for (auto [defI, cfg] : llvm_cfg) {
        cfg->BuildCFG();
    }
}

void CFG::BuildCFG() {
    // TODO("BuildCFG");
    // 标记那些块是可达的

    auto it = (*block_map).end();
    --it;
    this->max_label = it->first;

    // 如果不可达则值为 -1, 否则该值为跳转/返回指令在指令数组中的位置
    std::vector<int> reachable(max_label + 1, -1);

    // 新的映射表，只存储可达的基本块和指令
    std::map<int, LLVMBlock> new_map;

    // old-label -> new-label 的映射表
    std::map<int, int> old2new;
    int new_label = 0;

    // DFS 遍历
    std::stack<LLVMBlock> st;
    st.push((*block_map)[0]);
    while (!st.empty()) {
        auto bb = st.top();
        st.pop();
        auto label = bb->block_id;
        auto ins_list = bb->Instruction_list;
        if (reachable[label] != -1) {
            continue;
        }
        rev_ord.insert(rev_ord.begin(), label);
        int i = 0;
        for (auto &ins : ins_list) {
            if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_COND) {
                BrCondInstruction *br_ins = dynamic_cast<BrCondInstruction *>(ins);
                auto true_label = dynamic_cast<LabelOperand *>(br_ins->GetTrueLabel())->GetLabelNo();
                auto false_label = dynamic_cast<LabelOperand *>(br_ins->GetFalseLabel())->GetLabelNo();

                auto true_bb = (*block_map)[true_label];
                auto false_bb = (*block_map)[false_label];

                /*
                    br i1 %r10, label %L5, label %L6
                L5:  ;
                    br label %L7
                L6:  ;
                    br label %L7
                */
                // 跳转的基本块只有一条无跳转指令，那么直接删除该基本块，将本指令的跳转目标修改为 L7
                if (true_bb->Instruction_list.size() == 1 &&
                    true_bb->Instruction_list.back()->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_UNCOND) {
                    auto true_next_op =
                    dynamic_cast<BrUncondInstruction *>(true_bb->Instruction_list.back())->GetDestLabel();
                    br_ins->SetTrueLabel(true_next_op);
                    true_label = dynamic_cast<LabelOperand *>(true_next_op)->GetLabelNo();
                }
                if (false_bb->Instruction_list.size() == 1 &&
                    false_bb->Instruction_list.back()->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_UNCOND) {
                    auto false_next_op =
                    dynamic_cast<BrUncondInstruction *>(false_bb->Instruction_list.back())->GetDestLabel();
                    br_ins->SetFalseLabel(false_next_op);
                    false_label = dynamic_cast<LabelOperand *>(false_next_op)->GetLabelNo();
                }

                if (true_label == false_label) {
                    st.push((*block_map)[true_label]);
                } else {
                    st.push((*block_map)[true_label]);
                    st.push((*block_map)[false_label]);
                }

                break;

            } else if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_UNCOND) {
                auto br_ins = dynamic_cast<BrUncondInstruction *>(ins);
                auto next_label = dynamic_cast<LabelOperand *>(br_ins->GetDestLabel())->GetLabelNo();

                auto next_bb = (*block_map)[next_label];
                st.push(next_bb);

                break;
            } else if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::RET) {
                break;
            }
            i++;
        }

        reachable[label] = i;
        // 新的指令数组, i 位置为跳转/返回指令在指令数组中的位置
        auto new_list = std::deque<Instruction>(ins_list.begin(), ins_list.begin() + i + 1);
        bb->Instruction_list = new_list;
        new_map[new_label] = bb;
        old2new[label] = new_label++;
    }

    G.clear();
    invG.clear();
    G.resize(new_label);
    invG.resize(new_label);

    // 删除了一些只有无条件跳转指令的基本块，减少了 label 数量，为了减少内存开销，遍历所有跳转指令重新分配 label 编号
    for (auto &[lb, bb] : new_map) {
        auto &ins = bb->Instruction_list[reachable[bb->block_id]];
        bb->block_id = lb;
        if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_COND) {
            BrCondInstruction *br_ins = dynamic_cast<BrCondInstruction *>(ins);
            auto old_true = dynamic_cast<LabelOperand *>(br_ins->GetTrueLabel())->GetLabelNo();
            auto old_false = dynamic_cast<LabelOperand *>(br_ins->GetFalseLabel())->GetLabelNo();
            auto new_true = old2new[old_true];
            auto new_false = old2new[old_false];
            br_ins->SetTrueLabel(GetNewLabelOperand(new_true));
            br_ins->SetFalseLabel(GetNewLabelOperand(new_false));

            if (old_true == old_false) {
                G[lb].push_back((*block_map)[old_true]);
                invG[new_true].push_back(bb);
            } else {
                G[lb].push_back((*block_map)[old_true]);
                G[lb].push_back((*block_map)[old_false]);
                invG[new_true].push_back(bb);
                invG[new_false].push_back(bb);
            }

        } else if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_UNCOND) {
            BrUncondInstruction *br_ins = dynamic_cast<BrUncondInstruction *>(ins);
            auto old_dst = dynamic_cast<LabelOperand *>(br_ins->GetDestLabel())->GetLabelNo();
            auto new_dst = old2new[old_dst];
            br_ins->SetDstLabel(GetNewLabelOperand(new_dst));

            G[lb].push_back((*block_map)[old_dst]);
            invG[new_dst].push_back(bb);
        }
    }

    // 将新的 label-bb 映射表赋值给 cfg
    *block_map = new_map;
    this->max_label = new_label - 1;

    // 更新逆序图
    for (auto &i : rev_ord) {
        i = old2new[i];
    }
}

std::vector<LLVMBlock> CFG::GetPredecessor(LLVMBlock B) { return invG[B->block_id]; }

std::vector<LLVMBlock> CFG::GetPredecessor(int bbid) { return invG[bbid]; }

std::vector<LLVMBlock> CFG::GetSuccessor(LLVMBlock B) { return G[B->block_id]; }

std::vector<LLVMBlock> CFG::GetSuccessor(int bbid) { return G[bbid]; }