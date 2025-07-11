#ifndef IR_H
#define IR_H

#include "Instruction.h"
#include "cfg.h"
#include <map>

class LLVMIR {
public:
    // 使用的最大 reg 编号
    std::map<FuncDefInstruction, int> def_reg{};

    // 全局变量定义指令
    std::vector<Instruction> global_def{};
    // 函数声明指令
    std::vector<Instruction> function_declare{};

    // key为函数定义指令(需要保证函数定义指令与函数一一对应), value为函数对应的cfg
    // 在中间代码生成中, 你可以暂时忽略该成员变量, 你可以在代码优化时再考虑该变量
    std::map<FuncDefInstruction, CFG *> llvm_cfg{};

    // key为函数定义指令, value为函数对应的所有基本块, 该成员变量会在输出中使用，所以你必须对该变量正确赋值
    // 你必须保证函数的入口基本块为0号基本块，否则在后端会出现错误。
    std::map<FuncDefInstruction, std::map<int, LLVMBlock>> function_block_map;

    // 我们用函数定义指令来对应一个函数
    // 在LLVMIR中新建一个函数
    void NewFunction(FuncDefInstruction I) { function_block_map[I] = {}; }

    // 获取一个在函数I中编号为now_label的基本块, 该基本块必须已存在
    LLVMBlock GetBlock(FuncDefInstruction I, int now_label) { return function_block_map[I][now_label]; }

    // 在函数I中新建一个新的编号为x的基本块, 该编号不能与已经有的重复
    LLVMBlock NewBlock(FuncDefInstruction I, int x) {
        function_block_map[I][x] = new BasicBlock(x);
        return GetBlock(I, x);
    }
    void printIR(std::ostream &s);

    void CFGInit();
    void BuildCFG();
};

#endif
