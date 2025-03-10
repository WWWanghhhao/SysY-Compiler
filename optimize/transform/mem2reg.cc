#include "mem2reg.h"
#include <algorithm>
#include <deque>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

static std::map<PhiInstruction *, int> phi_map;
static std::set<int> common_allocas;

// 检查该条alloca指令是否可以被mem2reg
// eg. 数组不可以mem2reg
// eg. 如果该指针直接被使用不可以mem2reg(在SysY一般不可能发生,SysY不支持指针语法)
void Mem2RegPass::IsPromotable(CFG *C, Instruction AllocaInst) {
    // 判断是否是alloca指令
    if (AllocaInst->GetOpcode() != BasicInstruction::LLVMIROpcode::ALLOCA) {
        return;    // 不是alloca指令，直接返回
    }

    // 获取所有基本块中的指令列表
    auto &block_list = C->block_map;

    // 存储所有alloca的指令及其状态（是否有load操作）
    std::set<Instruction> allocas_to_delete;

    // 遍历基本块中的指令，标记无用的alloca指令
    for (auto &block_pair : *block_list) {
        auto &inst_list = block_pair.second->Instruction_list;
        for (auto &inst : inst_list) {
            // 如果指令是alloca并且没有被使用
            if (inst->GetOpcode() == BasicInstruction::LLVMIROpcode::ALLOCA) {
                bool is_used = false;

                // 检查是否有load指令使用了该alloca分配的内存
                for (auto &use_inst : inst_list) {
                    if (use_inst->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD) {
                        // 如果该alloca被加载，则认为它被使用过
                        is_used = true;
                        break;
                    }
                }

                // 如果没有被使用，标记该alloca指令为待删除
                if (!is_used) {
                    allocas_to_delete.insert(inst);
                }
            }
        }
    }

    // 创建新的指令列表，仅保留未被标记删除的指令
    for (auto &block_pair : *block_list) {
        auto &inst_list = block_pair.second->Instruction_list;
        std::deque<Instruction> new_inst_list;

        for (auto &inst : inst_list) {
            if (allocas_to_delete.find(inst) == allocas_to_delete.end()) {
                // 只有没有被删除的指令才加入新的列表
                new_inst_list.push_back(inst);
            }
        }

        // 更新基本块的指令列表
        block_pair.second->Instruction_list = new_inst_list;
    }
}

/*
    int a1 = 5,a2 = 3,a3 = 11,b = 4
    return b // a1,a2,a3 is useless
-----------------------------------------------
pseudo IR is:
    %r0 = alloca i32 ;a1
    %r1 = alloca i32 ;a2
    %r2 = alloca i32 ;a3
    %r3 = alloca i32 ;b
    store 5 -> %r0 ;a1 = 5
    store 3 -> %r1 ;a2 = 3
    store 11 -> %r2 ;a3 = 11
    store 4 -> %r3 ;b = 4
    %r4 = load i32 %r3
    ret i32 %r4
--------------------------------------------------
%r0,%r1,%r2只有store, 但没有load,所以可以删去
优化后的IR(pseudo)为:
    %r3 = alloca i32
    store 4 -> %r3
    %r4 - load i32 %r3
    ret i32 %r4
*/

// vset is the set of alloca regno that only store but not load
// 该函数对你的时间复杂度有一定要求, 你需要保证你的时间复杂度小于等于O(nlognlogn), n为该函数的指令数
// 提示:deque直接在中间删除是O(n)的, 可以先标记要删除的指令, 最后想一个快速的方法统一删除

void Mem2RegPass::Mem2RegNoUseAlloca(CFG *C, std::set<int> &vset) {
    std::vector<int> allocas;
    std::map<int, bool> used_alloc;
    used_alloc.clear();
    for (auto ins : (*C->block_map)[0]->Instruction_list) {
        if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::ALLOCA) {
            // 标记所有的 alloca 指令对应的 reg
            auto alloc_ins = (AllocaInstruction *)ins;
            auto op = alloc_ins->GetResult();
            if (alloc_ins->GetDims().size() == 0 && op && op->GetOperandType() == BasicOperand::REG) {
                auto reg = ((RegOperand *)op)->GetRegNo();
                allocas.push_back(reg);
                used_alloc[reg] = false;
            }
        }
    }

    for (auto [id, bb] : (*C->block_map)) {
        if (id == 0) {
            continue;
        }
        for (auto ins : bb->Instruction_list) {
            if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD) {
                auto load_ins = (LoadInstruction *)ins;
                auto op = load_ins->GetPointer();
                if (op && op->GetOperandType() == BasicOperand::REG) {
                    auto target_reg = ((RegOperand *)op)->GetRegNo();
                    if (std::find(allocas.begin(), allocas.end(), target_reg) != allocas.end()) {
                        // 如果有 load 该 reg，则有 use 该 alloca
                        used_alloc[target_reg] = true;
                    }
                }
            }
        }
    }

    std::map<int, std::set<int>> needDel;
    int index = -1;
    for (auto [id, bb] : (*C->block_map)) {
        index = -1;
        for (auto ins : bb->Instruction_list) {
            index++;
            if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::ALLOCA) {
                auto alloc_ins = (AllocaInstruction *)ins;
                auto op = alloc_ins->GetResult();
                if (alloc_ins->GetDims().size() == 0 && op && op->GetOperandType() == BasicOperand::REG) {
                    auto reg = ((RegOperand *)op)->GetRegNo();
                    if (used_alloc[reg] == false) {
                        needDel[id].insert(index);
                    }
                }
            }
            if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::STORE) {
                auto st_ins = (StoreInstruction *)ins;
                auto op = st_ins->GetPointer();
                if (op && op->GetOperandType() == BasicOperand::REG) {
                    auto target_reg = ((RegOperand *)op)->GetRegNo();
                    if (std::find(allocas.begin(), allocas.end(), target_reg) != allocas.end()) {
                        if (used_alloc[target_reg] == false) {
                            needDel[id].insert(index);
                        }
                    }
                }
            }
        }
    }

    // 执行删除
    for (auto [label, dels] : needDel) {
        for (auto it = dels.rbegin(); it != dels.rend(); ++it) {
            auto index = *it;
            (*C->block_map)[label]->Instruction_list.erase((*C->block_map)[label]->Instruction_list.begin() + index);
        }
    }
}

void Mem2RegPass::Mem2RegUseDefInSameBlock(CFG *C, std::set<int> &vset1, int block_id) {
    std::map<int, std::set<int>> defs, uses;    // 变量在哪些块中定义、变量在哪些块中使用
    std::map<int, int> def_num;                 // 变量定义次数
    std::map<int, std::set<int>> needDel;
    std::map<int, int> replace_map;
    std::map<int, std::set<int>> allocaUseDefInSameBlock;    //<blockid,<alloca regno>>

    for (auto [id, BB] : *C->block_map) {
        for (auto I : BB->Instruction_list) {
            if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::STORE) {
                auto StoreI = (StoreInstruction *)I;
                if (StoreI->GetPointer()->GetOperandType() == BasicOperand::GLOBAL) {
                    continue;    // 跳过全局变量
                }
                defs[StoreI->GetDefRegNo()].insert(id);
                def_num[StoreI->GetDefRegNo()]++;
            } else if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD) {
                auto LoadI = (LoadInstruction *)I;
                if (LoadI->GetPointer()->GetOperandType() == BasicOperand::GLOBAL) {
                    continue;
                }
                uses[LoadI->GetUseRegNo()].insert(id);
            }
        }
    }

    int index = -1;
    LLVMBlock entry_BB = (*C->block_map)[0];
    for (auto I : entry_BB->Instruction_list) {
        index++;
        if (I->GetOpcode() != BasicInstruction::LLVMIROpcode::ALLOCA) {
            continue;
        }

        // 排除array
        auto AllocaI = (AllocaInstruction *)I;
        if (!(AllocaI->GetDims().empty())) {
            continue;
        }
        int v = AllocaI->GetResultRegNo();

        auto alloca_defs = defs[v];
        auto alloca_uses = uses[v];

        // 判断是否在同一个block中
        if (alloca_defs.size() == 1) {
            int block_id = *(alloca_defs.begin());
            if (alloca_uses.size() == 1 && *(alloca_uses.begin()) == block_id) {
                needDel[0].insert(index);
                allocaUseDefInSameBlock[block_id].insert(v);
                continue;
            }
        }
    }
    for (auto [id, vset] : allocaUseDefInSameBlock) {
        // 记录alloca寄存器最近被赋值的寄存器号
        std::map<int, int> curr_reg_map;
        index = -1;
        for (auto I : (*C->block_map)[id]->Instruction_list) {
            index++;
            if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::STORE) {
                auto StoreI = (StoreInstruction *)I;
                if (StoreI->GetPointer()->GetOperandType() != BasicOperand::REG) {
                    continue;
                }
                int v = StoreI->GetDefRegNo();
                if (vset.find(v) == vset.end()) {
                    continue;
                }
                curr_reg_map[v] = ((RegOperand *)(StoreI->GetValue()))->GetRegNo();
                needDel[id].insert(index);
            }
            if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD) {
                auto LoadI = (LoadInstruction *)I;
                if (LoadI->GetPointer()->GetOperandType() != BasicOperand::REG) {
                    continue;
                }
                int v = LoadI->GetUseRegNo();
                if (vset.find(v) == vset.end()) {
                    continue;
                }
                replace_map[LoadI->GetResultRegNo()] = curr_reg_map[v];
                needDel[id].insert(index);
            }
        }
    }

    for (auto [id, bb] : *C->block_map) {
        for (auto I : bb->Instruction_list) {
            if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD &&
                ((LoadInstruction *)I)->GetPointer()->GetOperandType() == BasicOperand::REG) {
                auto LoadI = (LoadInstruction *)I;
                int result = LoadI->GetResultRegNo();
                if (replace_map.find(result) != replace_map.end()) {
                    int result2 = replace_map[result];
                    while (replace_map.find(result2) != replace_map.end()) {
                        replace_map[result] = replace_map[result2];
                        result2 = replace_map[result];
                    }
                }
            }
        }
    }

    // 删除指令
    for (auto [label, dels] : needDel) {
        for (auto it = dels.rbegin(); it != dels.rend(); ++it) {
            auto index = *it;
            (*C->block_map)[label]->Instruction_list.erase((*C->block_map)[label]->Instruction_list.begin() + index);
        }
    }

    // 寄存器号替换
    for (auto [id, BB] : *C->block_map) {
        for (auto I : BB->Instruction_list) {
            I->ReplaceRegByMap(replace_map);    // 替换寄存器号
        }
    }
}

// vset is the set of alloca regno that one store dominators all load instructions
// 该函数对你的时间复杂度有一定要求，你需要保证你的时间复杂度小于等于O(nlognlogn)
void Mem2RegPass::Mem2RegOneDefDomAllUses(CFG *C, std::set<int> &vset1) {
    // Step 1: Calculate defs, uses, and def_num
    std::map<int, std::set<int>> defs, uses;    // 变量在哪些块中定义、变量在哪些块中使用
    std::map<int, int> def_num;                 // 变量定义次数
    std::map<int, std::set<int>> needDel;
    std::map<int, int> mem2reg_map;
    auto domtree = domtrees->GetDomTree(C);

    for (auto [id, BB] : *C->block_map) {
        for (auto I : BB->Instruction_list) {
            if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::STORE) {
                auto StoreI = (StoreInstruction *)I;
                if (StoreI->GetPointer()->GetOperandType() == BasicOperand::GLOBAL) {
                    continue;    // 跳过全局变量
                }
                defs[StoreI->GetDefRegNo()].insert(id);
                def_num[StoreI->GetDefRegNo()]++;
            } else if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD) {
                auto LoadI = (LoadInstruction *)I;
                if (LoadI->GetPointer()->GetOperandType() == BasicOperand::GLOBAL) {
                    continue;
                }
                uses[LoadI->GetUseRegNo()].insert(id);
            }
        }
    }

    // 确定一个 store 是否支配所有 load
    std::set<int> vset;    // Set of variables to optimize
    LLVMBlock entry_BB = (*C->block_map)[0];
    int index = -1;
    for (auto I : entry_BB->Instruction_list) {
        index++;
        if (I->GetOpcode() != BasicInstruction::LLVMIROpcode::ALLOCA) {
            continue;
        }

        auto AllocaI = (AllocaInstruction *)I;
        if (!(AllocaI->GetDims().empty())) {
            continue;
        }    // 跳过数组
        int v = AllocaI->GetResultRegNo();

        auto alloca_defs = defs[v];
        auto alloca_uses = uses[v];
        if (def_num[v] == 1) {    // 只能定义数为 1 才可能是支配其它所有uses的store
            int block_id = *(alloca_defs.begin());
            int dom_flag = 1;

            // 如果所有uses所在的block都受该block的支配，该寄存器号才能加入vset
            for (auto load_BBid : alloca_uses) {
                if (domtree->IsDominate(block_id, load_BBid) == false) {
                    dom_flag = 0;
                    break;
                }
            }
            if (dom_flag) {    // one def dominate all uses
                needDel[0].insert(index);

                vset.insert(v);
                continue;
            }
        }
    }

    // 执行寄存器替换
    std::map<int, int> v_result_map;    // 变量最后一个store对应的value的寄存器号
    for (auto [id, BB] : *C->block_map) {
        index = -1;
        for (auto I : BB->Instruction_list) {
            index++;
            if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::STORE) {
                auto StoreI = (StoreInstruction *)I;
                if (StoreI->GetPointer()->GetOperandType() != BasicOperand::REG) {
                    continue;
                }
                int v = StoreI->GetDefRegNo();
                if (vset.find(v) == vset.end()) {
                    continue;
                }
                v_result_map[v] = ((RegOperand *)(StoreI->GetValue()))->GetRegNo();
                needDel[id].insert(index);
            }
        }
    }

    for (auto [id, BB] : *C->block_map) {
        index = -1;
        for (auto I : BB->Instruction_list) {
            index++;
            if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD) {
                auto LoadI = (LoadInstruction *)I;
                if (LoadI->GetPointer()->GetOperandType() != BasicOperand::REG) {
                    continue;
                }
                int v = LoadI->GetUseRegNo();
                // 跳过不在vset中的变量
                if (vset.find(v) == vset.end()) {
                    continue;
                }
                mem2reg_map[LoadI->GetResultRegNo()] = v_result_map[v];
                needDel[id].insert(index);
            }
        }
    }

    // 处理 LOAD 指令的间接映射
    for (auto [id, bb] : *C->block_map) {
        for (auto I : bb->Instruction_list) {
            if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD &&
                ((LoadInstruction *)I)->GetPointer()->GetOperandType() == BasicOperand::REG) {
                int result = ((LoadInstruction *)I)->GetResultRegNo();

                // 检查目标寄存器是否在重命名映射中
                if (mem2reg_map.find(result) != mem2reg_map.end()) {
                    int result2 = mem2reg_map[result];    // 获取目标寄存器的映射

                    // 处理间接映射：如果映射的寄存器本身也有映射，递归找到最终的寄存器编号
                    while (mem2reg_map.find(result2) != mem2reg_map.end()) {
                        mem2reg_map[result] = mem2reg_map[result2];
                        result2 = mem2reg_map[result];
                    }
                }
            }
        }
    }

    // 删除指令
    for (auto [label, dels] : needDel) {
        for (auto it = dels.rbegin(); it != dels.rend(); ++it) {
            auto index = *it;
            (*C->block_map)[label]->Instruction_list.erase((*C->block_map)[label]->Instruction_list.begin() + index);
        }
    }

    // 寄存器号替换
    for (auto B1 : *C->block_map) {
        for (auto I : B1.second->Instruction_list) {
            I->ReplaceRegByMap(mem2reg_map);
        }
    }

    mem2reg_map.clear();
}

// 辅助函数: 计算 defs_id 和 var2defs
void Mem2RegPass::FindDefs(CFG *c) {
    // 记录有哪些变量
    std::vector<int> vars_reg_no;
    defs_id.clear();
    var2defs.clear();

    for (auto [id, bb] : *c->block_map) {
        for (auto ins : bb->Instruction_list) {
            if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::ALLOCA) {
                auto AllocaI = (AllocaInstruction *)ins;
                if (!(AllocaI->GetDims().empty())) {
                    continue;
                }
                auto op = dynamic_cast<AllocaInstruction *>(ins)->GetResult();
                auto reg_no = dynamic_cast<RegOperand *>(op)->GetRegNo();
                var2defs[reg_no].clear();
                defs_id[reg_no].clear();
                vars_reg_no.push_back(reg_no);
                common_allocas.insert(reg_no);
            }
            if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::STORE) {
                auto op_type = dynamic_cast<StoreInstruction *>(ins)->GetPointer()->GetOperandType();
                if (op_type == BasicOperand::GLOBAL) {
                    continue;
                }
                auto reg_no =
                dynamic_cast<RegOperand *>(dynamic_cast<StoreInstruction *>(ins)->GetPointer())->GetRegNo();
                if (std::find(vars_reg_no.begin(), vars_reg_no.end(), reg_no) != vars_reg_no.end()) {
                    auto new_val_reg =
                    dynamic_cast<RegOperand *>(dynamic_cast<StoreInstruction *>(ins)->GetValue())->GetRegNo();
                    // new_val_reg 是对 var 的重新赋值的寄存器
                    var2defs[reg_no][id] = new_val_reg;
                    defs_id[reg_no].insert(id);
                }
            }
        }
    }
}

void Mem2RegPass::InsertPhi(CFG *C) {
    // TODO("InsertPhi");

    FindDefs(C);
    auto domtree = domtrees->GetDomTree(C);

    auto alloc_bb = (*C->block_map)[0];
    for (auto ins : alloc_bb->Instruction_list) {
        if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::ALLOCA) {
            auto AllocaI = (AllocaInstruction *)ins;
            if (!(AllocaI->GetDims().empty())) {
                continue;
            }
            auto type = dynamic_cast<AllocaInstruction *>(ins)->GetDataType();
            auto op = dynamic_cast<AllocaInstruction *>(ins)->GetResult();
            auto reg_no = dynamic_cast<RegOperand *>(op)->GetRegNo();
            if (!defs_id[reg_no].empty()) {
                auto defs = var2defs[reg_no];

                std::set<int> F{};                    // 添加 phi 指令的块
                std::set<int> W = defs_id[reg_no];    // 对 reg_no 定义的块

                while (!W.empty()) {
                    auto it = W.begin();
                    auto X = *it;
                    W.erase(it);
                    // 获取 X 的支配边界
                    for (auto Y : domtree->GetDF(X)) {
                        if (std::find(F.begin(), F.end(), Y) == F.end()) {
                            // 如果该支配边界没有插入 phi 则插入
                            auto phi = new PhiInstruction(type, GetNewRegOperand(++C->max_reg));
                            (*C->block_map)[Y]->InsertInstruction(0, phi);
                            phi_map[phi] = reg_no;
                            F.insert(Y);
                            if (std::find(defs_id[reg_no].begin(), defs_id[reg_no].end(), Y) == defs_id[reg_no].end()) {
                                // 添加该块为对 reg_no 的定义集合中
                                W.insert(Y);
                            }
                        }
                    }
                }
            }
        }
    }
}

// 辅助函数: 判断 load/store 指令是否指向 alloca 的 reg
int pointer2allocas(Instruction ins) {
    if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD ||
        ins->GetOpcode() == BasicInstruction::LLVMIROpcode::STORE) {
        Operand pointer;
        if ((LoadInstruction *)ins) {
            pointer = ((LoadInstruction *)ins)->GetPointer();
        } else {
            pointer = ((StoreInstruction *)ins)->GetPointer();
        }
        if (pointer->GetOperandType() != BasicOperand::REG) {
            return -1;
        }
        int pointer_reg = ((RegOperand *)pointer)->GetRegNo();
        if (common_allocas.find(pointer_reg) != common_allocas.end()) {
            return pointer_reg;
        }
    }
    return -1;
}

void Mem2RegPass::VarRename(CFG *C) {
    // for (auto i :common_allocas){
    // std::cout << i;}
    // std::cout << "   ";
    //  存储当前基本块及其对应的变量-寄存器映射（Incoming Values）
    std::map<int, std::map<int, int>> WorkList;    //< BB, <alloca_reg, val_reg> >

    WorkList[0] = {};    // 初始化第一个基本块

    std::map<int, std::set<Instruction>> wait_proc;

    // 标记基本块是否已访问
    std::vector<int> BBvis(C->max_label + 1, 0);

    std::map<int, std::set<int>> needDel;
    std::map<int, int> mem2reg_map;
    BBvis.resize(C->max_label + 1);
    int index = -1;
    while (!WorkList.empty()) {
        int BB = (*WorkList.begin()).first;
        auto IncomingVals = (*WorkList.begin()).second;
        WorkList.erase(BB);
        if (BBvis[BB]) {
            continue;
        }
        // BBvis[BB] = 1;
        if (wait_proc.find(BB) != wait_proc.end()) {
            index = -1;
            for (auto &I : (*C->block_map)[BB]->Instruction_list) {
                index++;
                if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD) {
                    auto LoadI = (LoadInstruction *)I;
                    int v = pointer2allocas(I);

                    // 如果 LOAD 指令操作的是 common_allocas 中的变量
                    if (v >= 0 && IncomingVals.find(v) != IncomingVals.end()) {
                        needDel[BB].insert(index);    // 标记该 LOAD 指令为待删除
                        mem2reg_map[LoadI->GetResultRegNo()] = IncomingVals[v];
                    } else {
                        wait_proc[BB].insert(I);
                    }
                } else if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::STORE) {
                    auto StoreI = (StoreInstruction *)I;
                    int v = pointer2allocas(I);

                    // 如果 STORE 指令操作的是 common_allocas 中的变量
                    if (v >= 0) {
                        // 标记该 STORE 指令为待删除
                        needDel[BB].insert(index);
                        IncomingVals[v] = ((RegOperand *)(StoreI->GetValue()))->GetRegNo();
                    }
                } else if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::PHI) {
                    auto PhiI = (PhiInstruction *)I;

                    // 如果 PHI 指令已经被标记为待删除，跳过
                    if (std::find(needDel[BB].begin(), needDel[BB].end(), index) != needDel[BB].end()) {
                        continue;
                    }
                    auto it = phi_map.find(PhiI);

                    // 如果 PHI 指令与 defs_id 相关联
                    if (it != phi_map.end()) {
                        IncomingVals[it->second] = PhiI->GetResultRegNo();
                    }
                }
            }
        } else {
            index = -1;
            for (auto &I : (*C->block_map)[BB]->Instruction_list) {
                index++;
                if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD &&
                    wait_proc[BB].find(I) != wait_proc[BB].end()) {
                    auto LoadI = (LoadInstruction *)I;
                    int v = pointer2allocas(I);

                    // 如果 LOAD 指令操作的是 common_allocas 中的变量
                    if (v >= 0 && IncomingVals.find(v) != IncomingVals.end()) {
                        // 标记该 LOAD 指令为待删除
                        needDel[BB].insert(index);

                        mem2reg_map[LoadI->GetResultRegNo()] = IncomingVals[v];
                        wait_proc[BB].erase(I);
                    } else {
                        wait_proc[BB].insert(I);
                    }
                }
            }
        }

        if (wait_proc[BB].empty()) {
            BBvis[BB] = 1;
            wait_proc.erase(BB);
        }

        // 处理当前基本块的后继块
        for (auto succ : C->G[BB]) {
            int BBv = succ->block_id;

            WorkList.insert({BBv, IncomingVals});

            index = -1;
            // 处理后继块中的PHI指令
            for (auto I : (*C->block_map)[BBv]->Instruction_list) {
                index++;
                if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::PHI) {
                    auto PhiI = dynamic_cast<PhiInstruction *>(I);
                    auto it = phi_map.find(PhiI);
                    if (it != phi_map.end()) {
                        int v = it->second;
                        if (IncomingVals.find(v) == IncomingVals.end()) {
                            needDel[BBv].insert(index);    // 删除未初始化的PHI指令
                        } else {
                            PhiI->InsertPhi(std::make_pair(GetNewLabelOperand(BB), GetNewRegOperand(IncomingVals[v])));
                        }
                    }
                }
            }
        }
    }

    // 处理 LOAD 指令的间接映射
    for (auto [id, bb] : *C->block_map) {
        for (auto I : bb->Instruction_list) {
            if (I->GetOpcode() == BasicInstruction::LLVMIROpcode::LOAD &&
                ((LoadInstruction *)I)->GetPointer()->GetOperandType() == BasicOperand::REG) {
                int result = ((LoadInstruction *)I)->GetResultRegNo();

                // 检查目标寄存器是否在重命名映射中
                if (mem2reg_map.find(result) != mem2reg_map.end()) {
                    int result2 = mem2reg_map[result];    // 获取目标寄存器的映射

                    // 处理间接映射：如果映射的寄存器本身也有映射，递归找到最终的寄存器编号
                    while (mem2reg_map.find(result2) != mem2reg_map.end()) {
                        mem2reg_map[result] = mem2reg_map[result2];
                        result2 = mem2reg_map[result];
                    }
                }
            }
        }
    }

    // for (auto [a,b]:mem2reg_map)
    //{std::cout<<"("<<a<<","<<b<<")   ";}

    // 删除待删除的指令
    for (auto [label, dels] : needDel) {
        for (auto it = dels.rbegin(); it != dels.rend(); ++it) {
            auto index = *it;
            (*C->block_map)[label]->Instruction_list.erase((*C->block_map)[label]->Instruction_list.begin() + index);
        }
    }

    // 替换指令中寄存器编号
    for (auto B1 : *C->block_map) {
        for (auto I : B1.second->Instruction_list) {
            I->ReplaceRegByMap(mem2reg_map);
        }
    }

    mem2reg_map.clear();
    phi_map.clear();
    common_allocas.clear();
}

void Mem2RegPass::Mem2Reg(CFG *C) {
    InsertPhi(C);
    // VarRename(C);
}

void Mem2RegPass::Execute() {
    int a;
    for (auto [defI, cfg] : llvmIR->llvm_cfg) {
        DCE(cfg);
        std::set<int> vset;
        Mem2RegNoUseAlloca(cfg, vset);
        Mem2RegOneDefDomAllUses(cfg, vset);
        Mem2RegUseDefInSameBlock(cfg, vset, a);
        Mem2RegOneDefDomAllUses(cfg, vset);
        Mem2RegNoUseAlloca(cfg, vset);
        Mem2Reg(cfg);
        VarRename(cfg);
        Mem2RegNoUseAlloca(cfg, vset);
        DCE(cfg);
    }
}

// 辅助函数: 计算 varUsed, varDefines, vars
void Mem2RegPass::FindDefsAndUses_DCE(CFG *c) {
    varUsed.clear();
    varDefines.clear();
    vars.clear();

    // offset 是指令在块内的下标
    int offset = 0;
    for (auto [id, bb] : *c->block_map) {
        offset = 0;
        for (auto ins : bb->Instruction_list) {
            if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::GETELEMENTPTR) {
                auto gep = ins->GetModified();
                // 为了防止 gep 指令被删除，将本身指令作为 gep 的 use
                varUsed[gep].insert(ins);
            }

            // Modified: 该指令修改的变量
            auto Modified = ins->GetModified();
            if (Modified) {
                varDefines[Modified].push_back(std::make_pair(id, offset));
                vars.insert(Modified);
            }

            // uses: 该指令使用的变量
            auto uses = ins->GetUses();
            for (auto op : uses) {
                if ((RegOperand *)op) {
                    varUsed[op].insert(ins);
                    vars.insert(op);
                }
            }
            offset++;
        }
    }
}

/*
对于局部数组，如果没对其赋初始值，并且没使用，则删除该局部数组
int arr[..][..];
保留给数组赋值语句(包括局部数组和传入的数组参数)
*/
void Mem2RegPass::DCE(CFG *c) {
    FindDefsAndUses_DCE(c);
    // 需要删除的指令表 key:label, value:块中需要删除的指令下标
    std::map<int, std::set<int>> needDel;
    // 初始化 worklist 为所有Alloca声明的变量
    auto workList = vars;
    while (!workList.empty()) {
        // 从 workList 删除变量 var
        auto var = *(workList.begin());
        workList.erase(workList.begin());
        // 如果 var 没有被使用
        if (varUsed[var].empty()) {
            auto defines = varDefines[var];
            for (auto def : defines) {
                // ins 是对 var 赋值的指令
                auto ins = (*c->block_map)[def.first]->Instruction_list[def.second];
                if (ins->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_COND ||
                    ins->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_UNCOND ||
                    ins->GetOpcode() == BasicInstruction::LLVMIROpcode::RET ||
                    ins->GetOpcode() == BasicInstruction::LLVMIROpcode::CALL ||
                    ins->GetOpcode() == BasicInstruction::LLVMIROpcode::GETELEMENTPTR ||
                    ins->GetModified()->GetOperandType() == BasicOperand::GLOBAL) {
                    continue;
                }
                // 删除 ins
                needDel[def.first].insert(def.second);
                for (auto x : ins->GetUses()) {
                    // 从 x 的使用列表中删除 ins
                    if (varUsed[x].find(ins) != varUsed[x].end()) {
                        varUsed[x].erase(varUsed[x].find(ins));
                    }
                    // ins 使用的变量可能会因为删除 ins 导致其使用列表为空，因此加入到 workList 中处理
                    workList.insert(x);
                }
            }
        }
    }
    // 执行删除
    for (auto [label, dels] : needDel) {
        for (auto it = dels.rbegin(); it != dels.rend(); ++it) {
            auto index = *it;
            // std::cout << label << " " << index << "\n";
            (*c->block_map)[label]->Instruction_list.erase((*c->block_map)[label]->Instruction_list.begin() + index);
        }
    }
}