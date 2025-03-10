#ifndef RISCV64_LOWERFRAME_H
#define RISCV64_LOWERFRAME_H

#include "../riscv64.h"
#include <vector>

class RiscV64LowerFrame : public MachinePass {
public:
    RiscV64LowerFrame(MachineUnit *unit) : MachinePass(unit) {}
    void Execute();
    Register fromImmF(float immf, MachineBlock *bb) {
        Register tmp = current_func->GetNewReg(INT64);
        // lui t0, %hi(1066192077)  # 加载高20位
        // addi t0, t0, %lo(1066192077)  # 加载低12位
        unsigned int *p = reinterpret_cast<unsigned int *>(&immf);
        auto binRep = *p;                          // 浮点数的二进制表示
        auto high20 = (binRep >> 12) & 0xFFFFF;    // 提取高20位
        auto low12 = binRep & 0xFFF;               // 提取低12位

        auto lui_inst = rvconstructor->ConstructUImm(RISCV_LUI, tmp, high20);
        bb->push_back(lui_inst);
        while (low12 > 2047) {
            // addi 立即数范围是 -2048 到 2047
            auto addi_inst = rvconstructor->ConstructIImm(RISCV_ADDI, tmp, tmp, 2047);
            bb->push_back(addi_inst);
            low12 -= 2047;
        }
        auto addi_inst = rvconstructor->ConstructIImm(RISCV_ADDI, tmp, tmp, low12);
        bb->push_back(addi_inst);

        auto fp_reg = current_func->GetNewReg(FLOAT64);
        auto s2fp_inst = rvconstructor->ConstructR2(RISCV_FMV_W_X, fp_reg, tmp);
        bb->push_back(s2fp_inst);
        return fp_reg;
    }
};

class RiscV64LowerStack : public MachinePass {
public:
    RiscV64LowerStack(MachineUnit *unit) : MachinePass(unit) {}
    void Execute();
};

#endif    // RISCV64_LOWERFRAME_H