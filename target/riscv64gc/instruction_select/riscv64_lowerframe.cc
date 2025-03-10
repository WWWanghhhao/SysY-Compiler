#include "riscv64_lowerframe.h"
#include "../../common/machine_instruction_structures/machine.h"

#ifdef PRINT_DBG
#include "../instruction_print/riscv64_printer.h"
#endif

constexpr int save_regids[] = {
RISCV_s0,  RISCV_s1,  RISCV_s2,  RISCV_s3,  RISCV_s4,   RISCV_s5,   RISCV_s6,  RISCV_s7,  RISCV_s8,
RISCV_s9,  RISCV_s10, RISCV_s11, RISCV_fs0, RISCV_fs1,  RISCV_fs2,  RISCV_fs3, RISCV_fs4, RISCV_fs5,
RISCV_fs6, RISCV_fs7, RISCV_fs8, RISCV_fs9, RISCV_fs10, RISCV_fs11, RISCV_ra,
};
constexpr int saveregnum = 25;
/*
    假设IR中的函数定义为f(i32 %r0, i32 %r1)
    则parameters应该存放两个虚拟寄存器%0,%1

    在LowerFrame后应当为
    add %0,a0,x0  (%0 <- a0)
    add %1,a1,x0  (%1 <- a1)

    对于浮点寄存器按照类似的方法处理即可
*/

void RiscV64LowerFrame::Execute() {
    for (auto func : unit->functions) {
        current_func = func;
        for (auto &b : func->blocks) {
            cur_block = b;
            if (b->getLabelId() == 0) {
                Register para_basereg = current_func->GetNewReg(INT64);
                int i32_cnt = 0;
                int f32_cnt = 0;
                int para_offset = 0;
                for (auto para : func->GetParameters()) {
                    if (para.type.data_type == INT64.data_type) {
                        if (i32_cnt < 8) {
                            b->push_front(
                            rvconstructor->ConstructR(RISCV_ADD, para, GetPhysicalReg(RISCV_a0 + i32_cnt),
                                                      GetPhysicalReg(RISCV_x0)));
                        }
                        if (i32_cnt >= 8) {
                            b->push_front(
                            rvconstructor->ConstructIImm(RISCV_LD, para, GetPhysicalReg(RISCV_fp), para_offset));
                            para_offset += 8;
                        }
                        i32_cnt++;
                    } else if (para.type.data_type == FLOAT64.data_type) {
                        if (f32_cnt < 8) {
                            auto fp_zero = current_func->GetNewReg(FLOAT64);
                            auto s2fp_inst =
                            rvconstructor->ConstructR2(RISCV_FMV_W_X, fp_zero, GetPhysicalReg(RISCV_x0));
                            b->push_front(rvconstructor->ConstructR(RISCV_FADD_S, para,
                                                                    GetPhysicalReg(RISCV_fa0 + f32_cnt), fp_zero));
                            b->push_front(s2fp_inst);
                        }
                        if (f32_cnt >= 8) {
                            b->push_front(
                            rvconstructor->ConstructIImm(RISCV_FLD, para, GetPhysicalReg(RISCV_fp), para_offset));
                            para_offset += 8;
                        }
                        f32_cnt++;
                    } else {
                        ERROR("Unknown type");
                    }
                }

                if (para_offset != 0) {
                    current_func->SetHasInParaInStack(true);
                    cur_block->push_front(
                    rvconstructor->ConstructIImm(RISCV_ADDIW, para_basereg, GetPhysicalReg(RISCV_fp), 0));
                }
            }
        }
    }
}

void GatherUseSregs(MachineFunction *func, std::vector<std::vector<int>> &reg_defblockids,
                    std::vector<std::vector<int>> &reg_rwblockids) {
    reg_defblockids.resize(64);
    reg_rwblockids.resize(64);
    for (auto &b : func->blocks) {
        int RegNeedSaved[64] = {
        [RISCV_s0] = 1,  [RISCV_s1] = 1,  [RISCV_s2] = 1,   [RISCV_s3] = 1,   [RISCV_s4] = 1,
        [RISCV_s5] = 1,  [RISCV_s6] = 1,  [RISCV_s7] = 1,   [RISCV_s8] = 1,   [RISCV_s9] = 1,
        [RISCV_s10] = 1, [RISCV_s11] = 1, [RISCV_fs0] = 1,  [RISCV_fs1] = 1,  [RISCV_fs2] = 1,
        [RISCV_fs3] = 1, [RISCV_fs4] = 1, [RISCV_fs5] = 1,  [RISCV_fs6] = 1,  [RISCV_fs7] = 1,
        [RISCV_fs8] = 1, [RISCV_fs9] = 1, [RISCV_fs10] = 1, [RISCV_fs11] = 1, [RISCV_ra] = 1,
        };
        for (auto ins : *b) {
            for (auto reg : ins->GetWriteReg()) {
                if (reg->is_virtual == false) {
                    if (RegNeedSaved[reg->reg_no]) {
                        RegNeedSaved[reg->reg_no] = 0;
                        reg_defblockids[reg->reg_no].push_back(b->getLabelId());
                        reg_rwblockids[reg->reg_no].push_back(b->getLabelId());
                    }
                }
            }
            for (auto reg : ins->GetReadReg()) {
                if (reg->is_virtual == false) {
                    if (RegNeedSaved[reg->reg_no]) {
                        RegNeedSaved[reg->reg_no] = 0;
                        reg_rwblockids[reg->reg_no].push_back(b->getLabelId());
                    }
                }
            }
        }
    }
    if (func->HasInParaInStack()) {
        reg_defblockids[RISCV_fp].push_back(0);
        reg_rwblockids[RISCV_fp].push_back(0);
    }
}

void GatherUseSregs(MachineFunction *func, std::vector<int> &saveregs) {
    int RegNeedSaved[64] = {
    [RISCV_s0] = 1,  [RISCV_s1] = 1,  [RISCV_s2] = 1,   [RISCV_s3] = 1,   [RISCV_s4] = 1,
    [RISCV_s5] = 1,  [RISCV_s6] = 1,  [RISCV_s7] = 1,   [RISCV_s8] = 1,   [RISCV_s9] = 1,
    [RISCV_s10] = 1, [RISCV_s11] = 1, [RISCV_fs0] = 1,  [RISCV_fs1] = 1,  [RISCV_fs2] = 1,
    [RISCV_fs3] = 1, [RISCV_fs4] = 1, [RISCV_fs5] = 1,  [RISCV_fs6] = 1,  [RISCV_fs7] = 1,
    [RISCV_fs8] = 1, [RISCV_fs9] = 1, [RISCV_fs10] = 1, [RISCV_fs11] = 1, [RISCV_ra] = 1,
    };
    for (auto &b : func->blocks) {
        for (auto ins : *b) {
            for (auto reg : ins->GetWriteReg()) {
                if (reg->is_virtual == false) {
                    if (RegNeedSaved[reg->reg_no]) {
                        RegNeedSaved[reg->reg_no] = 0;
                        saveregs.push_back(reg->reg_no);
                    }
                }
            }
        }
    }
    // save fp
    if (func->HasInParaInStack()) {
        if (RegNeedSaved[RISCV_fp]) {
            RegNeedSaved[RISCV_fp] = 0;
            saveregs.push_back(RISCV_fp);
        }
    }
}

void RiscV64LowerStack::Execute() {
    // 在函数在寄存器分配后执行
    // TODO: 在函数开头保存 函数被调者需要保存的寄存器，并开辟栈空间
    // TODO: 在函数结尾恢复 函数被调者需要保存的寄存器，并收回栈空间
    // TODO: 开辟和回收栈空间
    // 具体需要保存/恢复哪些可以查阅RISC-V函数调用约定

    // Log("RiscV64LowerStack");
    for (auto func : unit->functions) {
        current_func = func;
        std::vector<std::vector<int>> saveregs_occurblockids, saveregs_rwblockids;
        GatherUseSregs(func, saveregs_occurblockids, saveregs_rwblockids);

        int saveregnum = 0;
        for (int i = 0; i < saveregs_occurblockids.size(); i++) {
            if (!saveregs_rwblockids[i].empty()) {
                saveregnum++;
            }
        }
        func->AddStackSize(saveregnum * 8);

        for (auto &b : func->blocks) {
            cur_block = b;
            if (b->getLabelId() == 0) {
                if (func->GetStackSize() <= 2032) {
                    b->push_front(rvconstructor->ConstructIImm(RISCV_ADDI, GetPhysicalReg(RISCV_sp),
                                                               GetPhysicalReg(RISCV_sp),
                                                               -func->GetStackSize()));    // sub sp
                } else {
                    auto stacksz_reg = GetPhysicalReg(RISCV_t0);
                    b->push_front(rvconstructor->ConstructR(RISCV_SUB, GetPhysicalReg(RISCV_sp),
                                                            GetPhysicalReg(RISCV_sp), stacksz_reg));
                    b->push_front(rvconstructor->ConstructUImm(RISCV_LI, stacksz_reg, func->GetStackSize()));
                }
                if (func->HasInParaInStack()) {
                    b->push_front(rvconstructor->ConstructR(RISCV_ADD, GetPhysicalReg(RISCV_fp),
                                                            GetPhysicalReg(RISCV_sp), GetPhysicalReg(RISCV_x0)));
                }

                int offset = 0;
                for (int i = 0; i < 64; i++) {
                    if (!saveregs_occurblockids[i].empty()) {
                        int regno = i;
                        offset -= 8;
                        if (regno >= RISCV_x0 && regno <= RISCV_x31) {
                            b->push_front(rvconstructor->ConstructSImm(RISCV_SD, GetPhysicalReg(regno),
                                                                       GetPhysicalReg(RISCV_sp), offset));
                        } else {
                            b->push_front(rvconstructor->ConstructSImm(RISCV_FSD, GetPhysicalReg(regno),
                                                                       GetPhysicalReg(RISCV_sp), offset));
                        }
                    }
                }
            }

            auto last_ins = *(b->ReverseBegin());

            auto riscv_last_ins = (RiscV64Instruction *)last_ins;
            if (riscv_last_ins->getOpcode() == RISCV_JALR) {
                if (riscv_last_ins->getRd() == GetPhysicalReg(RISCV_x0)) {
                    if (riscv_last_ins->getRs1() == GetPhysicalReg(RISCV_ra)) {
                        b->pop_back();
                        if (func->GetStackSize() <= 2032) {
                            b->push_back(rvconstructor->ConstructIImm(RISCV_ADDI, GetPhysicalReg(RISCV_sp),
                                                                      GetPhysicalReg(RISCV_sp), func->GetStackSize()));
                        } else {
                            auto stacksz_reg = GetPhysicalReg(RISCV_t0);
                            b->push_back(rvconstructor->ConstructUImm(RISCV_LI, stacksz_reg, func->GetStackSize()));
                            b->push_back(rvconstructor->ConstructR(RISCV_ADD, GetPhysicalReg(RISCV_sp),
                                                                   GetPhysicalReg(RISCV_sp), stacksz_reg));
                        }

                        int offset = 0;
                        for (int i = 0; i < 64; i++) {
                            if (!saveregs_occurblockids[i].empty()) {
                                int regno = i;
                                offset -= 8;
                                if (regno >= RISCV_x0 && regno <= RISCV_x31) {
                                    b->push_back(rvconstructor->ConstructIImm(RISCV_LD, GetPhysicalReg(regno),
                                                                              GetPhysicalReg(RISCV_sp), offset));
                                } else {
                                    b->push_back(rvconstructor->ConstructIImm(RISCV_FLD, GetPhysicalReg(regno),
                                                                              GetPhysicalReg(RISCV_sp), offset));
                                }
                            }
                        }

                        b->push_back(riscv_last_ins);
                    }
                }
            }
        }
    }

    // 到此我们就完成目标代码生成的所有工作了
}