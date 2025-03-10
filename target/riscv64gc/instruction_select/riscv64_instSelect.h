#ifndef RISCV64_INSTSELECT_H
#define RISCV64_INSTSELECT_H
#include "../../common/machine_passes/machine_selector.h"
#include "../riscv64.h"
#include <map>
class RiscV64Selector : public MachineSelector {
private:
    int cur_offset;    // 局部变量在栈中的偏移

    // 你需要保证在每个函数的指令选择结束后, cur_offset的值为局部变量所占栈空间的大小

    // TODO(): 添加更多你需要的成员变量和函数

    /*
     中间代码中，判断条件和进行分支跳转是两条指令
        %r8 = icmp eq i32 %r3,%r5
        br i1 %r8, label %L3, label %L2
     而在RISC-V中需要在一条指令中给出(example: b(cond) Rs1, Rs2,label)，所以需要借助 r8
     来将第一条中间代码的条件传递给跳转指令
    */
    std::map<Operand, Instruction> cmp_map;

    // 记录变量是否分配了sp空间，value是sp内的偏移量
    std::map<Operand, int> alloca_off;

public:
    RiscV64Selector(MachineUnit *dest, LLVMIR *IR) : MachineSelector(dest, IR) {}
    void SelectInstructionAndBuildCFG();
    void ClearFunctionSelectState();
    template <class INSPTR> void ConvertAndAppend(INSPTR);

    // 返回加载了浮点立即数的整数寄存器
    Register fromImmF(float immf);
    Register fromImm(int imm);
};
#endif