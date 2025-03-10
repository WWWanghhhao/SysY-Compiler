#include "IRgen.h"
#include "../include/cfg.h"
#include "../include/ir.h"
#include "semant.h"
#include <cassert>
#include <cstddef>
#include <cstring>
#include <unordered_map>
#include <utility>
#include <vector>

extern SemantTable semant_table;    // 也许你会需要一些语义分析的信息

IRgenTable irgen_table;    // 中间代码生成的辅助变量
LLVMIR llvmIR;             // 我们需要在这个变量中生成中间代码

// 调试函数
void x() { std::cout << "\n"; }
void Here() {
    x();
    std::cout << "here\n";
}

// 从 Type 转换为 LLVMType
// enum ty { VOID = 0, INT = 1, FLOAT = 2, BOOL = 3, PTR = 4, DOUBLE = 5 } type;
const BasicInstruction::LLVMType getLLVMType[5] = {
BasicInstruction::VOID, BasicInstruction::I32, BasicInstruction::FLOAT32, BasicInstruction::I1, BasicInstruction::PTR};

Type::ty getType(BasicInstruction::LLVMType t) {
    if (t == BasicInstruction::I1) {
        return Type::BOOL;
    } else if (t == BasicInstruction::I32) {
        return Type::INT;
    } else if (t == BasicInstruction::FLOAT32) {
        return Type::FLOAT;
    } else if (t == BasicInstruction::PTR) {
        return Type::PTR;
    } else {
        return Type::VOID;
    }
}

// label 编号
int cur_label = -1;
int NewLabel() {
    cur_label++;
    return cur_label;
}
// reg 编号
int cur_reg = -1;
int NewReg() {
    cur_reg++;
    return cur_reg;
}

// 维护一个 cfg 用来标识当前位于的 function, blocks, current label
CFG cur_cfg;

// 函数返回 label
int return_label;

void AddLibFunctionDeclare();

// 在基本块B末尾生成一条新指令
void IRgenArithmeticI32(LLVMBlock B, BasicInstruction::LLVMIROpcode opcode, int reg1, int reg2, int result_reg);
void IRgenArithmeticF32(LLVMBlock B, BasicInstruction::LLVMIROpcode opcode, int reg1, int reg2, int result_reg);
void IRgenArithmeticI32ImmLeft(LLVMBlock B, BasicInstruction::LLVMIROpcode opcode, int val1, int reg2, int result_reg);
void IRgenArithmeticF32ImmLeft(LLVMBlock B, BasicInstruction::LLVMIROpcode opcode, float val1, int reg2,
                               int result_reg);
void IRgenArithmeticI32ImmAll(LLVMBlock B, BasicInstruction::LLVMIROpcode opcode, int val1, int val2, int result_reg);
void IRgenArithmeticF32ImmAll(LLVMBlock B, BasicInstruction::LLVMIROpcode opcode, float val1, float val2,
                              int result_reg);

void IRgenIcmp(LLVMBlock B, BasicInstruction::IcmpCond cmp_op, int reg1, int reg2, int result_reg);
void IRgenFcmp(LLVMBlock B, BasicInstruction::FcmpCond cmp_op, int reg1, int reg2, int result_reg);
void IRgenIcmpImmRight(LLVMBlock B, BasicInstruction::IcmpCond cmp_op, int reg1, int val2, int result_reg);
void IRgenFcmpImmRight(LLVMBlock B, BasicInstruction::FcmpCond cmp_op, int reg1, float val2, int result_reg);

void IRgenFptosi(LLVMBlock B, int src, int dst);
void IRgenSitofp(LLVMBlock B, int src, int dst);
void IRgenZextI1toI32(LLVMBlock B, int src, int dst);

void IRgenGetElementptrIndexI32(LLVMBlock B, BasicInstruction::LLVMType type, int result_reg, Operand ptr,
                                std::vector<int> dims, std::vector<Operand> indexs);

void IRgenGetElementptrIndexI64(LLVMBlock B, BasicInstruction::LLVMType type, int result_reg, Operand ptr,
                                std::vector<int> dims, std::vector<Operand> indexs);

void IRgenLoad(LLVMBlock B, BasicInstruction::LLVMType type, int result_reg, Operand ptr);
void IRgenStore(LLVMBlock B, BasicInstruction::LLVMType type, int value_reg, Operand ptr);
void IRgenStore(LLVMBlock B, BasicInstruction::LLVMType type, Operand value, Operand ptr);

void IRgenCall(LLVMBlock B, BasicInstruction::LLVMType type, int result_reg,
               std::vector<std::pair<enum BasicInstruction::LLVMType, Operand>> args, std::string name);
void IRgenCallVoid(LLVMBlock B, BasicInstruction::LLVMType type,
                   std::vector<std::pair<enum BasicInstruction::LLVMType, Operand>> args, std::string name);

void IRgenCallNoArgs(LLVMBlock B, BasicInstruction::LLVMType type, int result_reg, std::string name);
void IRgenCallVoidNoArgs(LLVMBlock B, BasicInstruction::LLVMType type, std::string name);

void IRgenRetReg(LLVMBlock B, BasicInstruction::LLVMType type, int reg);
void IRgenRetImmInt(LLVMBlock B, BasicInstruction::LLVMType type, int val);
void IRgenRetImmFloat(LLVMBlock B, BasicInstruction::LLVMType type, float val);
void IRgenRetVoid(LLVMBlock B);

void IRgenBRUnCond(LLVMBlock B, int dst_label);
void IRgenBrCond(LLVMBlock B, int cond_reg, int true_label, int false_label);

void IRgenAlloca(LLVMBlock B, BasicInstruction::LLVMType type, int reg);
void IRgenAllocaArray(LLVMBlock B, BasicInstruction::LLVMType type, int reg, std::vector<int> dims);

RegOperand *GetNewRegOperand(int RegNo);

// generate TypeConverse Instructions from type_src to type_dst
// eg. you can use fptosi instruction to converse float to int
// eg. you can use zext instruction to converse bool to int
void IRgenTypeConverse(LLVMBlock B, Type::ty type_src, Type::ty type_dst, int src, int dst) {
    // TODO("IRgenTypeConverse. Implement it if you need it");

    if (type_dst == type_src) {
        cur_reg--;
        return;
    }
    if (type_src == Type::INT) {
        if (type_dst == Type::FLOAT) {
            IRgenSitofp(B, src, NewReg());
        } else if (type_dst == Type::BOOL) {
            IRgenIcmpImmRight(B, BasicInstruction::IcmpCond::ne, src, 0, NewReg());
        }

    } else if (type_src == Type::FLOAT) {
        if (type_dst == Type::INT) {
            IRgenFptosi(B, src, NewReg());
        } else if (type_dst == Type::BOOL) {
            IRgenFcmpImmRight(B, BasicInstruction::FcmpCond::ONE, src, 0, NewReg());
        }

    } else if (type_src == Type::BOOL) {
        if (type_dst == Type::INT) {
            IRgenZextI1toI32(B, src, NewReg());
        } else if (type_dst == Type::FLOAT) {
            IRgenZextI1toI32(B, src, NewReg());
            IRgenSitofp(B, cur_reg, NewReg());
        }
    } else {
        cur_reg--;
    }
}

void BasicBlock::InsertInstruction(int pos, Instruction Ins) {
    assert(pos == 0 || pos == 1);
    if (pos == 0) {
        Instruction_list.push_front(Ins);
    } else if (pos == 1) {
        Instruction_list.push_back(Ins);
    }
}

/*
二元运算指令生成的伪代码：
    假设现在的语法树节点是：AddExp_plus
    该语法树表示 addexp + mulexp

    addexp->codeIR()
    mulexp->codeIR()
    假设mulexp生成完后，我们应该在基本块B0继续插入指令。
    addexp的结果存储在r0寄存器中，mulexp的结果存储在r1寄存器中
    生成一条指令r2 = r0 + r1，并将该指令插入基本块B0末尾。
    标注后续应该在基本块B0插入指令，当前节点的结果寄存器为r2。
    (如果考虑支持浮点数，需要查看语法树节点的类型来判断此时是否需要隐式类型转换)
*/

void __Program::codeIR() {
    AddLibFunctionDeclare();
    auto comp_vector = *comp_list;
    for (auto comp : comp_vector) {
        comp->codeIR();
    }
}

void Exp::codeIR() { addexp->codeIR(); }

const Type::ty GetCorrectType[5][5]{{Type::VOID, Type::VOID, Type::VOID, Type::VOID, Type::VOID},
                                    {Type::VOID, Type::INT, Type::FLOAT, Type::INT, Type::VOID},
                                    {Type::VOID, Type::FLOAT, Type::FLOAT, Type::FLOAT, Type::VOID},
                                    {Type::VOID, Type::INT, Type::FLOAT, Type::BOOL, Type::VOID},
                                    {Type::VOID, Type::VOID, Type::VOID, Type::VOID, Type::VOID}};

void AddExp_plus::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    addexp->codeIR();
    int r1 = cur_reg;
    mulexp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[addexp->attribute.T.type][mulexp->attribute.T.type];
    if (t == Type::INT) {
        if (addexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (mulexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::ADD, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (addexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (mulexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (mulexp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenArithmeticF32(bb, BasicInstruction::LLVMIROpcode::FADD, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::ADD, r1, r2, NewReg());
    }
}

void AddExp_sub::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    addexp->codeIR();
    int r1 = cur_reg;
    mulexp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[addexp->attribute.T.type][mulexp->attribute.T.type];
    if (t == Type::INT) {
        if (addexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (mulexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::SUB, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (addexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (mulexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (mulexp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenArithmeticF32(bb, BasicInstruction::LLVMIROpcode::FSUB, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::SUB, r1, r2, NewReg());
    }
}

void MulExp_mul::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    mulexp->codeIR();
    int r1 = cur_reg;
    unary_exp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[mulexp->attribute.T.type][unary_exp->attribute.T.type];
    if (t == Type::INT) {
        if (mulexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (unary_exp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::MUL, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (mulexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (mulexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (unary_exp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (unary_exp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenArithmeticF32(bb, BasicInstruction::LLVMIROpcode::FMUL, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::MUL, r1, r2, NewReg());
    }
}

void MulExp_div::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    mulexp->codeIR();
    int r1 = cur_reg;
    unary_exp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[mulexp->attribute.T.type][unary_exp->attribute.T.type];
    if (t == Type::INT) {
        if (mulexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (unary_exp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::DIV, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (mulexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (mulexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (unary_exp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (unary_exp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenArithmeticF32(bb, BasicInstruction::LLVMIROpcode::FDIV, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::DIV, r1, r2, NewReg());
    }
}

void MulExp_mod::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    mulexp->codeIR();
    int r1 = cur_reg;
    unary_exp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[mulexp->attribute.T.type][unary_exp->attribute.T.type];
    if (t == Type::INT) {
        if (mulexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (unary_exp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::MOD, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenArithmeticI32(bb, BasicInstruction::LLVMIROpcode::MUL, r1, r2, NewReg());
    }
}

void RelExp_leq::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    relexp->codeIR();
    int r1 = cur_reg;
    addexp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[relexp->attribute.T.type][addexp->attribute.T.type];
    if (t == Type::INT) {
        if (relexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenIcmp(bb, BasicInstruction::IcmpCond::sle, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (relexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (relexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (addexp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenFcmp(bb, BasicInstruction::FcmpCond::OLE, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenIcmp(bb, BasicInstruction::IcmpCond::sle, r1, r2, NewReg());
    }
}

void RelExp_lt::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    relexp->codeIR();
    int r1 = cur_reg;
    addexp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[relexp->attribute.T.type][addexp->attribute.T.type];
    if (t == Type::INT) {
        if (relexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenIcmp(bb, BasicInstruction::IcmpCond::slt, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (relexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (relexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (addexp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenFcmp(bb, BasicInstruction::FcmpCond::OLT, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenIcmp(bb, BasicInstruction::IcmpCond::slt, r1, r2, NewReg());
    }
}

void RelExp_geq::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    relexp->codeIR();
    int r1 = cur_reg;
    addexp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[relexp->attribute.T.type][addexp->attribute.T.type];
    if (t == Type::INT) {
        if (relexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenIcmp(bb, BasicInstruction::IcmpCond::sge, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (relexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (relexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (addexp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenFcmp(bb, BasicInstruction::FcmpCond::OGE, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenIcmp(bb, BasicInstruction::IcmpCond::sge, r1, r2, NewReg());
    }
}

void RelExp_gt::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    relexp->codeIR();
    int r1 = cur_reg;
    addexp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[relexp->attribute.T.type][addexp->attribute.T.type];
    if (t == Type::INT) {
        if (relexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenIcmp(bb, BasicInstruction::IcmpCond::sgt, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (relexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (relexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (addexp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenFcmp(bb, BasicInstruction::FcmpCond::OGT, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenIcmp(bb, BasicInstruction::IcmpCond::sgt, r1, r2, NewReg());
    }
}

void EqExp_eq::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    eqexp->codeIR();
    int r1 = cur_reg;
    relexp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[eqexp->attribute.T.type][relexp->attribute.T.type];
    if (t == Type::INT) {
        if (eqexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (relexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenIcmp(bb, BasicInstruction::IcmpCond::eq, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (eqexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (eqexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (relexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (relexp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenFcmp(bb, BasicInstruction::FcmpCond::OEQ, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenIcmp(bb, BasicInstruction::IcmpCond::eq, r1, r2, NewReg());
    }
}

void EqExp_neq::codeIR() {
    // TODO("BinaryExp CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    eqexp->codeIR();
    int r1 = cur_reg;
    relexp->codeIR();
    int r2 = cur_reg;

    auto t = GetCorrectType[eqexp->attribute.T.type][relexp->attribute.T.type];
    if (t == Type::INT) {
        if (eqexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (relexp->attribute.T.type == Type::BOOL) {
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenIcmp(bb, BasicInstruction::IcmpCond::ne, r1, r2, NewReg());
    } else if (t == Type::FLOAT) {
        if (eqexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r1, NewReg());
            r1 = cur_reg;
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (eqexp->attribute.T.type == Type::INT) {
            // int -> float
            IRgenSitofp(bb, r1, NewReg());
            r1 = cur_reg;
        } else if (relexp->attribute.T.type == Type::BOOL) {
            // bool -> int
            IRgenZextI1toI32(bb, r2, NewReg());
            r2 = cur_reg;
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        } else if (relexp->attribute.T.type == Type::INT) {
            IRgenSitofp(bb, r2, NewReg());
            r2 = cur_reg;
        }
        IRgenFcmp(bb, BasicInstruction::FcmpCond::ONE, r1, r2, NewReg());
    } else if (t == Type::BOOL) {
        IRgenZextI1toI32(bb, r1, NewReg());
        r1 = cur_reg;
        IRgenZextI1toI32(bb, r2, NewReg());
        r2 = cur_reg;
        IRgenIcmp(bb, BasicInstruction::IcmpCond::ne, r1, r2, NewReg());
    }
}

// short circuit &&
void LAndExp_and::codeIR() {
    // TODO("LAndExpAnd CodeIR");

    /*
    leftBB:
        decide to jump rightBB or false label
    rightBB:
        decide to jump true of false label

    true:
    false:
    */

    // 首先创建一个基本块 rightBB, 作为右表达式生成的指令插入的位置
    auto rightBB = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());
    // 设置左表达式的真假 label, 然后生成左表达式的中间代码
    landexp->true_label = rightBB->block_id;
    landexp->false_label = false_label;
    landexp->codeIR();

    auto leftBB = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenTypeConverse(leftBB, landexp->attribute.T.type, Type::ty::BOOL, cur_reg, NewReg());
    IRgenBrCond(leftBB, cur_reg, landexp->true_label, landexp->false_label);

    // 只有当左表达式为真时, 才执行右表达式
    // 获取右表达式生成的基本块
    rightBB = (*cur_cfg.block_map)[landexp->true_label];
    // 设置右表达式的真假, 设置当前 label, 然后生成右表达式的中间代码
    eqexp->true_label = true_label;
    eqexp->false_label = false_label;
    cur_cfg.func_cur_label = landexp->true_label;
    eqexp->codeIR();
    IRgenTypeConverse(rightBB, eqexp->attribute.T.type, Type::ty::BOOL, cur_reg, NewReg());
}

// short circuit ||
void LOrExp_or::codeIR() {
    // TODO("LOrExpOr CodeIR");

    /*
    leftBB:
        decide to jump true or rightBB
    rightBB:
        decide to jump true of false label

    true:
    false:
    */

    // 首先创建一个基本块 rightBB, 作为右表达式生成的指令插入的位置
    auto rightBB = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());
    // 设置左表达式的真假 label, 然后生成左表达式的中间代码
    lorexp->true_label = true_label;
    lorexp->false_label = rightBB->block_id;
    lorexp->codeIR();

    auto leftBB = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenTypeConverse(leftBB, lorexp->attribute.T.type, Type::ty::BOOL, cur_reg, NewReg());
    IRgenBrCond(leftBB, cur_reg, lorexp->true_label, lorexp->false_label);

    // 只有当左表达式为假时, 才执行右表达式
    // 获取右表达式生成的基本块
    rightBB = (*cur_cfg.block_map)[lorexp->false_label];
    // 设置右表达式的真假, 设置当前 label, 然后生成右表达式的中间代码
    landexp->true_label = true_label;
    landexp->false_label = false_label;
    cur_cfg.func_cur_label = lorexp->false_label;
    landexp->codeIR();
    IRgenTypeConverse(rightBB, landexp->attribute.T.type, Type::ty::BOOL, cur_reg, NewReg());
}

void ConstExp::codeIR() { addexp->codeIR(); }

void Lval::codeIR() {
    // TODO("Lval CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];

    VarAttribute def_var;
    bool isGlobal = false;
    int alloca_reg = irgen_table.symbol_table.lookup(name);
    if (alloca_reg != -1) {
        // 局部变量
        def_var = irgen_table.reg_table[alloca_reg];
    } else if (semant_table.GlobalTable.find(name) != semant_table.GlobalTable.end()) {
        // 全局变量
        isGlobal = true;
        def_var = semant_table.GlobalVarTable[name];
    }
    std::vector<Expression> dim_vector;
    std::vector<Operand> lval_dims;
    if (dims) {
        dim_vector = *dims;
    }
    for (auto d : dim_vector) {
        d->codeIR();
        IRgenTypeConverse(bb, d->attribute.T.type, Type::ty::INT, cur_reg, NewReg());
        lval_dims.push_back(GetNewRegOperand(cur_reg));
    }
    if (def_var.dims.size() > lval_dims.size()) {
        // 数组
        Operand op;
        // 使用数组的(name)首地址, 维度定义不为空, 使用为空, 或者 a[1]  a[2][3]
        if (isGlobal) {
            op = GetNewGlobalOperand(name->get_string());
        } else {
            op = GetNewRegOperand(alloca_reg);
        }
        lval_dims.insert(lval_dims.begin(), new ImmI32Operand(0));
        IRgenGetElementptrIndexI32(bb, getLLVMType[def_var.type], NewReg(), op, def_var.dims, lval_dims);

    } else if (def_var.dims.size() == lval_dims.size()) {
        // 非数组
        if (def_var.dims.size() == 0) {
            // 定义的值不是数组 或者 函数中继续调用形参数组
            // a = b;             int f(int a[]) { f(a); }
            Operand op;
            if (isGlobal) {
                op = GetNewGlobalOperand(name->get_string());
            } else {
                op = GetNewRegOperand(alloca_reg);
            }
            if (attribute.T.type != Type::PTR) {
                IRgenLoad(bb, getLLVMType[def_var.type], NewReg(), op);
            } else {
                IRgenGetElementptrIndexI32(bb, getLLVMType[def_var.type], NewReg(), op, def_var.dims,
                                           {new ImmI32Operand(0)});
            }
        } else {
            // 定义的值是数组
            Operand op;
            if (isGlobal) {
                op = GetNewGlobalOperand(name->get_string());
            } else {
                op = GetNewRegOperand(alloca_reg);
            }
            lval_dims.insert(lval_dims.begin(), new ImmI32Operand(0));
            IRgenGetElementptrIndexI32(bb, getLLVMType[def_var.type], NewReg(), op, def_var.dims, lval_dims);

            op = GetNewRegOperand(cur_reg);
            IRgenLoad(bb, getLLVMType[def_var.type], NewReg(), op);
        }
    } else if (def_var.dims.size() < lval_dims.size()) {
        // Here();
        // 形参中含有数组
        Operand op;
        if (isGlobal) {
            op = GetNewGlobalOperand(name->get_string());
        } else {
            op = GetNewRegOperand(alloca_reg);
        }
        IRgenGetElementptrIndexI32(bb, getLLVMType[def_var.type], NewReg(), op, def_var.dims, lval_dims);
        op = GetNewRegOperand(cur_reg);
        IRgenLoad(bb, getLLVMType[def_var.type], NewReg(), op);
    }
}

void FuncRParams::codeIR() { TODO("FuncRParams CodeIR"); }

void Func_call::codeIR() {
    // TODO("FunctionCall CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    auto f_def = semant_table.FunctionTable[name];

    if (funcr_params) {                                                      // 参数不为空
        std::vector<std::pair<BasicInstruction::LLVMType, Operand>> args;    // 参数列表
        auto formals = semant_table.FunctionTable[name]->formals;            // 形参
        auto reals = dynamic_cast<FuncRParams *>(funcr_params)->params;      // 实参
        for (int i = 0; i < reals->size(); i++) {
            auto formal = (*formals)[i];
            auto real = (*reals)[i];
            real->codeIR();
            IRgenTypeConverse(bb, real->attribute.T.type, formal->type_decl, cur_reg, NewReg());
            if (real->attribute.T.type == Type::PTR) {
                args.push_back(std::make_pair(getLLVMType[Type::PTR], GetNewRegOperand(cur_reg)));
            } else {
                args.push_back(std::make_pair(getLLVMType[formal->type_decl], GetNewRegOperand(cur_reg)));
            }
        }
        if (f_def->return_type != Type::VOID) {    // 有返回值函数
            IRgenCall(bb, getLLVMType[f_def->return_type], NewReg(), args, name->get_string());
        } else {    // 无返回值函数
            IRgenCallVoid(bb, BasicInstruction::LLVMType::VOID, args, name->get_string());
        }
    } else {    // 参数为空
        IRgenCallNoArgs(bb, getLLVMType[f_def->return_type], NewReg(), name->get_string());
    }
}

void UnaryExp_plus::codeIR() {
    // TODO("UnaryExpPlus CodeIR");

    // auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    unary_exp->codeIR();
}

void UnaryExp_neg::codeIR() {
    // TODO("UnaryExpNeg CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    unary_exp->codeIR();
    if (unary_exp->attribute.T.type == Type::INT) {
        // res_reg = 0 - cur_reg;
        IRgenArithmeticI32ImmLeft(bb, BasicInstruction::LLVMIROpcode::SUB, 0, cur_reg, NewReg());
    } else if (unary_exp->attribute.T.type == Type::FLOAT) {
        IRgenArithmeticF32ImmLeft(bb, BasicInstruction::LLVMIROpcode::FSUB, 0, cur_reg, NewReg());
    } else {
        IRgenZextI1toI32(bb, cur_reg, NewReg());
        IRgenArithmeticI32ImmLeft(bb, BasicInstruction::LLVMIROpcode::SUB, 0, cur_reg, NewReg());
    }
}

void UnaryExp_not::codeIR() {
    // TODO("UnaryExpNot CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    unary_exp->codeIR();
    if (unary_exp->attribute.T.type == Type::INT) {
        // res_reg = 0 == cur_reg;
        IRgenIcmpImmRight(bb, BasicInstruction::IcmpCond::eq, cur_reg, 0, NewReg());
    } else if (unary_exp->attribute.T.type == Type::FLOAT) {
        IRgenFcmpImmRight(bb, BasicInstruction::FcmpCond::OEQ, cur_reg, 0, NewReg());
    } else {
        IRgenZextI1toI32(bb, cur_reg, NewReg());
        IRgenIcmpImmRight(bb, BasicInstruction::IcmpCond::eq, cur_reg, 0, NewReg());
    }
}

void IntConst::codeIR() {
    // TODO("IntConst CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenArithmeticI32ImmAll(bb, BasicInstruction::LLVMIROpcode::ADD, val, 0, NewReg());
}

void FloatConst::codeIR() {
    // TODO("FloatConst CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenArithmeticF32ImmAll(bb, BasicInstruction::LLVMIROpcode::FADD, val, 0, NewReg());
}

void StringConst::codeIR() { TODO("StringConst CodeIR"); }

void PrimaryExp_branch::codeIR() { exp->codeIR(); }

void assign_stmt::codeIR() {
    // TODO("AssignStmt CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];

    lval->codeIR();

    // 对于 lval = exp 指令，会首先 load ptr lval，这条指令是不需要的bb->Instruction list.pop back()
    bb->Instruction_list.pop_back();
    auto l_exp = dynamic_cast<Lval *>(lval);
    // 查找 lval 声明时分配的寄存器
    auto l_reg = irgen_table.symbol_table.lookup(l_exp->name);
    exp->codeIR();

    IRgenTypeConverse(bb, exp->attribute.T.type, lval->attribute.T.type, cur_reg, NewReg());
    if (l_reg == -1) {
        // 全局变量
        int tmp = cur_reg;
        auto def_var = semant_table.GlobalTable[l_exp->name];
        Operand op;
        std::vector<Operand> l_dims;
        if (l_exp->dims != nullptr) {    // 是一个数组
            // 需要访问全局数组变量中的元素
            for (auto d : *l_exp->dims) {
                d->codeIR();
                IRgenTypeConverse(bb, d->attribute.T.type, Type::ty::INT, cur_reg, NewReg());
                l_dims.push_back(GetNewRegOperand(cur_reg));
            }
            // op = 全局变量数组首地址
            op = GetNewGlobalOperand(l_exp->name->get_string());
            l_dims.insert(l_dims.begin(), new ImmI32Operand(0));
            IRgenGetElementptrIndexI32(bb, getLLVMType[def_var.type], NewReg(), op, def_var.dims, l_dims);
            // op = 全局变量数组中元素的地址
            op = GetNewRegOperand(cur_reg);
        } else {    // 不是数组
            op = GetNewGlobalOperand(l_exp->name->get_string());
        }
        IRgenStore(bb, getLLVMType[l_exp->attribute.T.type], GetNewRegOperand(tmp), op);
    } else {    // 局部变量
        int tmp = cur_reg;
        Operand op;
        std::vector<Operand> l_dims;
        if (l_exp->dims != nullptr) {
            // 是一个数组
            auto def_var = irgen_table.reg_table[l_reg];    // 找到局部变量数组的变量属性
            if (l_exp->dims->size() <= def_var.dims.size()) {
                // 需要访问局部变量数组变量中的元素
                for (auto d : *l_exp->dims) {
                    d->codeIR();
                    IRgenTypeConverse(bb, d->attribute.T.type, Type::ty::INT, cur_reg, NewReg());
                    l_dims.push_back(GetNewRegOperand(cur_reg));
                }
                // op = 局部变量数组首地址
                op = GetNewRegOperand(l_reg);
                l_dims.insert(l_dims.begin(), new ImmI32Operand(0));
                IRgenGetElementptrIndexI32(bb, getLLVMType[def_var.type], NewReg(), op, def_var.dims, l_dims);
                // op = 局部变量数组中元素的地址
                op = GetNewRegOperand(cur_reg);
            } else if (l_exp->dims->size() > def_var.dims.size()) {    // 形参
                // 需要访问局部变量数组变量中的元素
                for (auto d : *l_exp->dims) {
                    d->codeIR();
                    IRgenTypeConverse(bb, d->attribute.T.type, Type::ty::INT, cur_reg, NewReg());
                    l_dims.push_back(GetNewRegOperand(cur_reg));
                }
                // op = 局部变量数组首地址
                op = GetNewRegOperand(l_reg);
                IRgenGetElementptrIndexI32(bb, getLLVMType[def_var.type], NewReg(), op, def_var.dims, l_dims);
                // op = 局部变量数组中元素的地址
                op = GetNewRegOperand(cur_reg);
            }
        } else {
            op = GetNewRegOperand(l_reg);
        }
        IRgenStore(bb, getLLVMType[l_exp->attribute.T.type], GetNewRegOperand(tmp), op);
    }
}

void expr_stmt::codeIR() { exp->codeIR(); }

void block_stmt::codeIR() {
    // TODO("BlockStmt CodeIR");

    irgen_table.symbol_table.enter_scope();
    b->codeIR();
    irgen_table.symbol_table.exit_scope();
}

void ifelse_stmt::codeIR() {
    // TODO("IfElseStmt CodeIR");

    /*
    cond_label:
        decide to jump if or else stmt
    if_stmt:
        ...
    else_stmt:
        ...
    */

    // ifBB 是 ifstmt 节点生成的指令的插入位置, elseBB 是 elsestmt 节点生成的指令插入位置, endBB 是 if
    // 语句后续的节点生成的指令的插入位置
    auto ifBB = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());
    auto elseBB = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());
    auto endBB = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());

    // 设置 Cond 的 true false label
    Cond->true_label = ifBB->block_id;
    Cond->false_label = elseBB->block_id;
    // cur_cfg.func_cur_label = endBB->block_id - 3;
    Cond->codeIR();

    // 获取 Cond 生成的基本块
    auto condBB = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenTypeConverse(condBB, Cond->attribute.T.type, Type::ty::BOOL, cur_reg, NewReg());
    IRgenBrCond(condBB, cur_reg, Cond->true_label, Cond->false_label);

    cur_cfg.func_cur_label = Cond->true_label;
    ifstmt->codeIR();
    ifBB = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenBRUnCond(ifBB, endBB->block_id);

    cur_cfg.func_cur_label = Cond->false_label;
    elsestmt->codeIR();
    elseBB = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenBRUnCond(elseBB, endBB->block_id);

    cur_cfg.func_cur_label = endBB->block_id;
    return_label = endBB->block_id;
}

void if_stmt::codeIR() {
    // TODO("IfStmt CodeIR");

    /*
    cond_label:
        decide to jump ture of false label
    true:
        ...
    false:
        ...
    */

    // ifBB 是 ifstmt 节点生成的指令的插入位置, endBB 是 if 语句后续的节点生成的指令的插入位置
    auto ifBB = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());
    auto endBB = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());

    // 设置 Cond 的 true false label
    Cond->true_label = ifBB->block_id;
    Cond->false_label = endBB->block_id;
    // cur_cfg.func_cur_label = endBB->block_id - 2;
    Cond->codeIR();

    // 获取 Cond 生成的基本块
    auto condBB = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    // std::cout << cond_label << "\n";
    IRgenTypeConverse(condBB, Cond->attribute.T.type, Type::ty::BOOL, cur_reg, NewReg());
    IRgenBrCond(condBB, cur_reg, Cond->true_label, Cond->false_label);

    cur_cfg.func_cur_label = Cond->true_label;
    ifstmt->codeIR();

    ifBB = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenBRUnCond(ifBB, Cond->false_label);

    cur_cfg.func_cur_label = Cond->false_label;
    return_label = Cond->false_label;
}

void while_stmt::codeIR() {
    // TODO("WhileStmt CodeIR");

    /*
    while语句指令生成的伪代码：
        while的语法树节点为while(cond)stmt

        假设当前我们应该在B0基本块开始插入指令
        新建三个基本块Bcond，Bbody，Bend
        在B0基本块末尾插入一条无条件跳转指令，跳转到Bcond

        设置当前我们应该在Bcond开始插入指令
        cond->codeIR()    //在调用该函数前你可能需要设置真假值出口
        假设cond生成完后，我们应该在B1基本块继续插入指令，Bcond的结果为r0
        如果r0的类型不为bool，在B1末尾生成一条比较语句，比较r0是否为真。
        在B1末尾生成一条条件跳转语句，如果为真，跳转到Bbody，如果为假，跳转到Bend

        设置当前我们应该在Bbody开始插入指令
        stmt->codeIR()
        假设当stmt生成完后，我们应该在B2基本块继续插入指令
        在B2末尾生成一条无条件跳转语句，跳转到Bcond

        设置当前我们应该在Bend开始插入指令
    */
    auto Bcond = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());
    auto Bbody = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());
    auto Bend = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());

    // 保存起始终止label，防止递归 while 时无法恢复
    int tmp1 = cur_cfg.loop_start_label, tmp2 = cur_cfg.loop_end_label;

    cur_cfg.loop_start_label = Bcond->block_id;
    cur_cfg.loop_end_label = Bend->block_id;

    auto B0 = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenBRUnCond(B0, Bcond->block_id);

    cur_cfg.func_cur_label = Bcond->block_id;
    Cond->true_label = Bbody->block_id;
    Cond->false_label = Bend->block_id;
    Cond->codeIR();

    auto B1 = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    if (Cond->attribute.T.type != Type::ty::BOOL) {
        IRgenTypeConverse(B1, Cond->attribute.T.type, Type::ty::BOOL, cur_reg, NewReg());
        IRgenBrCond(B1, cur_reg, Cond->true_label, Cond->false_label);
    } else {
        IRgenBrCond(B1, cur_reg, Cond->true_label, Cond->false_label);
    }

    cur_cfg.func_cur_label = Bbody->block_id;
    body->codeIR();

    auto B2 = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenBRUnCond(B2, Bcond->block_id);

    cur_cfg.loop_end_label = tmp2;
    cur_cfg.loop_start_label = tmp1;
    cur_cfg.func_cur_label = Bend->block_id;
    return_label = Bend->block_id;
}

void continue_stmt::codeIR() {
    // TODO("ContinueStmt CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenBRUnCond(bb, cur_cfg.loop_start_label);
    // cur_cfg.func_cur_label = cur_cfg.loop_start_label;
}

void break_stmt::codeIR() {
    // TODO("BreakStmt CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenBRUnCond(bb, cur_cfg.loop_end_label);
    // cur_cfg.func_cur_label = cur_cfg.loop_end_label;
}

void return_stmt::codeIR() {
    // TODO("ReturnStmt CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    return_exp->codeIR();

    IRgenTypeConverse(bb, return_exp->attribute.T.type, getType(cur_cfg.function_def->GetReturnType()), cur_reg,
                      NewReg());

    IRgenRetReg(bb, cur_cfg.function_def->GetReturnType(), cur_reg);
}

void return_stmt_void::codeIR() {
    // TODO("ReturnStmtVoid CodeIR");

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];
    IRgenRetVoid(bb);
}

void ConstInitVal::codeIR() { TODO("ConstInitVal CodeIR"); }

void ConstInitVal_exp::codeIR() {
    // TODO("ConstInitValWithExp CodeIR");

    exp->codeIR();
}

void VarInitVal::codeIR() { TODO("VarInitVal CodeIR"); }

void VarInitVal_exp::codeIR() {
    // TODO("VarInitValWithExp CodeIR");

    exp->codeIR();
}

void VarDef_no_init::codeIR() { TODO("VarDefNoInit CodeIR"); }

void VarDef::codeIR() { TODO("VarDef CodeIR"); }

void ConstDef::codeIR() { TODO("ConstDef CodeIR"); }

// 从展平数组的下标，得到原始的索引数组
std::vector<int> GetIndexFromFlatIndex(const std::vector<int> dims, int flatIndex) {
    /*
    a[4][5]
    1 -- [0][1]
    2 -- [0][2]
    ...
    5 -- [1][0]
    ...
    */

    std::vector<int> index(dims.size());
    int remaining = flatIndex;
    for (int i = dims.size() - 1; i >= 0; i--) {
        index[i] = remaining % dims[i];
        remaining /= dims[i];
    }
    return index;
}

size_t initIntArray(LLVMBlock bb, std::vector<InitVal> *currentInit, int addr_reg, const std::vector<int> &dims,
                    size_t index, int depth) {
    if (!currentInit) {
        return index;
    }

    // 提前计算下一维度的索引
    size_t elementsInDimension = 1;
    for (int i = depth; i < dims.size(); ++i) {
        elementsInDimension *= dims[i];
    }
    int next_index = elementsInDimension + index;

    // 遍历当前维度的所有初始化值
    for (auto val : *currentInit) {
        if (val->GetInitValArray()) {
            // 递归处理数组初始化
            index = initIntArray(bb, val->GetInitValArray(), addr_reg, dims, index, depth + 1);
        } else {
            // 生成IR代码
            val->codeIR();
            IRgenTypeConverse(bb, val->attribute.T.type, Type::INT, cur_reg, NewReg());
            auto val_reg = cur_reg;

            // 使用GetElementptrInstruction获取元素地址
            auto getele =
            new GetElementptrInstruction(BasicInstruction::LLVMType::I32, GetNewRegOperand(NewReg()),
                                         GetNewRegOperand(addr_reg), dims, BasicInstruction::LLVMType::I32);
            auto result = getele->GetResult();
            getele->push_idx_imm32(0);

            // 获取原始索引
            auto origin_indexes = GetIndexFromFlatIndex(dims, index++);
            for (auto i : origin_indexes) {
                getele->push_idx_imm32(i);
            }
            bb->InsertInstruction(1, getele);

            // 存储值
            IRgenStore(bb, getLLVMType[Type::INT], GetNewRegOperand(val_reg), result);
        }
    }

    return next_index;
}

size_t initFloatArray(LLVMBlock bb, std::vector<InitVal> *currentInit, int addr_reg, const std::vector<int> &dims,
                      size_t index, int depth) {

    if (!currentInit) {
        return index;
    }

    size_t elementsInDimension = 1;
    for (int i = depth; i < dims.size(); ++i) {
        elementsInDimension *= dims[i];
    }
    int next_index = elementsInDimension + index;
    for (auto val : *currentInit) {

        if (val->GetInitValArray()) {
            index = initIntArray(bb, val->GetInitValArray(), addr_reg, dims, index, depth + 1);
        } else {
            val->codeIR();
            IRgenTypeConverse(bb, val->attribute.T.type, Type::FLOAT, cur_reg, NewReg());
            auto val_reg = cur_reg;
            auto getele =
            new GetElementptrInstruction(BasicInstruction::LLVMType::FLOAT32, GetNewRegOperand(NewReg()),
                                         GetNewRegOperand(addr_reg), dims, BasicInstruction::LLVMType::I32);
            auto result = getele->GetResult();
            getele->push_idx_imm32(0);

            auto origin_indexes = GetIndexFromFlatIndex(dims, index++);
            for (auto i : origin_indexes) {
                getele->push_idx_imm32(i);
            }
            bb->InsertInstruction(1, getele);
            IRgenStore(bb, getLLVMType[Type::FLOAT], GetNewRegOperand(val_reg), result);
        }
    }

    return next_index;
}

void VarDecl::codeIR() {
    // TODO("VarDecl CodeIR");

    // 成员变量
    // Type::ty type_decl;
    // std::vector<Def> *var_def_list{};

    if (semant_table.symbol_table.get_current_scope() == 0) {
        // 全局变量声明
        for (auto def : *var_def_list) {
            VarAttribute var = semant_table.GlobalVarTable[def->GetName()];

            Instruction ins;
            if (var.dims.size() == 0) {    // 非数组
                auto init = def->GetInitVal();
                if (init != nullptr) {    // 有初始值
                    if (var.type == Type::INT) {
                        ins = new GlobalVarDefineInstruction(def->GetName()->get_string(), getLLVMType[var.type],
                                                             new ImmI32Operand(var.IntInitVals[0]));
                    } else if (var.type == Type::FLOAT) {
                        ins = new GlobalVarDefineInstruction(def->GetName()->get_string(), getLLVMType[var.type],
                                                             new ImmF32Operand(var.FloatInitVals[0]));
                    }
                } else {
                    ins = new GlobalVarDefineInstruction(def->GetName()->get_string(), getLLVMType[var.type], nullptr);
                }

            } else {    // 全局变量数组初始化
                ins = new GlobalVarDefineInstruction(def->GetName()->get_string(), getLLVMType[var.type], var);
            }
            llvmIR.global_def.push_back(ins);
        }
        return;
    }

    // 分配内存指令全都放在开始部分 alloc_b 执行
    auto alloc_b = (*cur_cfg.block_map)[0];

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];

    for (auto def : *var_def_list) {
        VarAttribute var;
        var.type = type_decl;

        // 该变量用来记录声明时为变量分配的 reg
        int vardecl_reg;

        if (def->GetDims()) {    // 数组
            for (auto d : *(def->GetDims())) {
                var.dims.push_back(d->attribute.V.val.IntVal);
            }
            irgen_table.symbol_table.add_Symbol(def->GetName(), NewReg());
            vardecl_reg = cur_reg;
            irgen_table.reg_table[cur_reg] = var;
            IRgenAllocaArray(alloc_b, getLLVMType[type_decl], cur_reg, var.dims);

            if (def->GetInitVal()) {
                size_t size = 1;
                for (auto d : var.dims) {
                    size *= d;
                }

                // FunctionDeclareInstruction *llvm_memset =
                // new FunctionDeclareInstruction(BasicInstruction::VOID, "llvm.memset.p0.i32");
                // llvm_memset->InsertFormal(BasicInstruction::PTR);
                // llvm_memset->InsertFormal(BasicInstruction::I8);
                // llvm_memset->InsertFormal(BasicInstruction::I32);
                // llvm_memset->InsertFormal(BasicInstruction::I1);

                std::vector<std::pair<BasicInstruction::LLVMType, Operand>> args;
                // 指向待初始化内存的指针
                args.push_back(std::make_pair(BasicInstruction::PTR, GetNewRegOperand(cur_reg)));
                // 初始值
                args.push_back(std::make_pair(BasicInstruction::I8, new ImmI32Operand(0)));
                // 初始化的长度，以字节为单位
                args.push_back(std::make_pair(BasicInstruction::I32, new ImmI32Operand(size * sizeof(int))));
                // 内存对齐长度
                args.push_back(std::make_pair(BasicInstruction::I1, new ImmI32Operand(0)));
                auto memset = new CallInstruction(BasicInstruction::LLVMType::VOID, GetNewRegOperand(cur_reg),
                                                  "llvm.memset.p0.i32", args);
                bb->InsertInstruction(1, memset);

                if (type_decl == Type::INT) {
                    size_t index = initIntArray(bb, def->GetInitVal()->GetInitValArray(), cur_reg, var.dims, 0, 0);
                } else if (type_decl == Type::FLOAT) {
                    size_t index = initFloatArray(bb, def->GetInitVal()->GetInitValArray(), cur_reg, var.dims, 0, 0);
                }
            }

        } else {
            // 添加符号表
            irgen_table.symbol_table.add_Symbol(def->GetName(), NewReg());
            vardecl_reg = cur_reg;
            // 记录寄存器和变量之间的关系
            irgen_table.reg_table[cur_reg] = var;
            // 分配内存
            IRgenAlloca(alloc_b, getLLVMType[type_decl], cur_reg);

            auto init = def->GetInitVal();
            if (init) {    // 如果有初始值
                auto v_exp = dynamic_cast<VarInitVal_exp *>(init);
                v_exp->exp->codeIR();
                IRgenTypeConverse(bb, init->attribute.T.type, type_decl, cur_reg, NewReg());
                auto operand = GetNewRegOperand(cur_reg);
                IRgenStore(bb, getLLVMType[type_decl], operand, GetNewRegOperand(vardecl_reg));
            }
            /*
            else {    // 没有初始值默认为 0
                if (type_decl == Type::INT) {
                    IRgenArithmeticI32ImmAll(bb, BasicInstruction::LLVMIROpcode::ADD, 0, 0, NewReg());
                } else if (type_decl == Type::FLOAT) {
                    IRgenArithmeticF32ImmAll(bb, BasicInstruction::LLVMIROpcode::FADD, 0, 0, NewReg());
                }
            }
            auto operand = GetNewRegOperand(cur_reg);
            IRgenStore(bb, getLLVMType[type_decl], operand, GetNewRegOperand(vardecl_reg));
            */
        }
    }
}

void ConstDecl::codeIR() {
    // TODO("ConstDecl CodeIR");

    if (semant_table.symbol_table.get_current_scope() == 0) {
        // 全局变量声明
        for (auto def : *var_def_list) {
            VarAttribute var = semant_table.GlobalVarTable[def->GetName()];

            Instruction ins;
            if (var.dims.size() == 0) {    // 非数组
                auto init = def->GetInitVal();
                if (init != nullptr) {    // 有初始值

                    if (var.type == Type::INT) {
                        ins = new GlobalVarDefineInstruction(def->GetName()->get_string(), getLLVMType[var.type],
                                                             new ImmI32Operand(var.IntInitVals[0]));
                    } else if (var.type == Type::FLOAT) {
                        ins = new GlobalVarDefineInstruction(def->GetName()->get_string(), getLLVMType[var.type],
                                                             new ImmF32Operand(var.FloatInitVals[0]));
                    }
                } else {
                    ins = new GlobalVarDefineInstruction(def->GetName()->get_string(), getLLVMType[var.type], nullptr);
                }

            } else {    // 全局变量数组初始化
                ins = new GlobalVarDefineInstruction(def->GetName()->get_string(), getLLVMType[var.type], var);
            }
            llvmIR.global_def.push_back(ins);
        }
        return;
    }

    // 分配内存指令全都放在开始部分 alloc_b 执行
    auto alloc_b = (*cur_cfg.block_map)[0];

    auto bb = (*cur_cfg.block_map)[cur_cfg.func_cur_label];

    for (auto def : *var_def_list) {
        VarAttribute var;
        var.type = type_decl;
        var.ConstTag = true;
        // 该变量用来记录声明时为变量分配的 reg
        int vardecl_reg;

        if (def->GetDims()) {    // 数组
            for (auto d : *(def->GetDims())) {
                var.dims.push_back(d->attribute.V.val.IntVal);
            }
            irgen_table.symbol_table.add_Symbol(def->GetName(), NewReg());
            vardecl_reg = cur_reg;
            irgen_table.reg_table[cur_reg] = var;
            IRgenAllocaArray(alloc_b, getLLVMType[type_decl], cur_reg, var.dims);

            if (def->GetInitVal()) {
                size_t size = 1;
                for (auto d : var.dims) {
                    size *= d;
                }
                std::vector<std::pair<BasicInstruction::LLVMType, Operand>> args;
                args.push_back(std::make_pair(BasicInstruction::PTR, GetNewRegOperand(cur_reg)));
                args.push_back(std::make_pair(BasicInstruction::I8, new ImmI32Operand(0)));
                args.push_back(std::make_pair(BasicInstruction::I32, new ImmI32Operand(size * sizeof(int))));
                args.push_back(std::make_pair(BasicInstruction::I1, new ImmI32Operand(0)));
                auto memset = new CallInstruction(BasicInstruction::LLVMType::VOID, GetNewRegOperand(cur_reg),
                                                  "llvm.memset.p0.i32", args);
                bb->InsertInstruction(1, memset);
                if (type_decl == Type::INT) {
                    size_t index = initIntArray(bb, def->GetInitVal()->GetInitValArray(), cur_reg, var.dims, 0, 0);

                } else if (type_decl == Type::FLOAT) {
                    size_t index = initFloatArray(bb, def->GetInitVal()->GetInitValArray(), cur_reg, var.dims, 0, 0);
                }
            }

        } else {
            // 添加符号表
            irgen_table.symbol_table.add_Symbol(def->GetName(), NewReg());
            vardecl_reg = cur_reg;
            // 记录寄存器和变量之间的关系
            irgen_table.reg_table[cur_reg] = var;
            // 分配内存
            IRgenAlloca(alloc_b, getLLVMType[type_decl], cur_reg);

            auto init = def->GetInitVal();
            if (init) {    // 如果有初始值
                auto c_exp = dynamic_cast<ConstInitVal_exp *>(init);
                c_exp->exp->codeIR();
                IRgenTypeConverse(bb, init->attribute.T.type, type_decl, cur_reg, NewReg());
            } else {    // 没有初始值默认为 0
                if (type_decl == Type::INT) {
                    IRgenArithmeticI32ImmAll(bb, BasicInstruction::LLVMIROpcode::ADD, 0, 0, NewReg());
                } else if (type_decl == Type::FLOAT) {
                    IRgenArithmeticF32ImmAll(bb, BasicInstruction::LLVMIROpcode::FADD, 0, 0, NewReg());
                }
            }
            auto operand = GetNewRegOperand(cur_reg);
            IRgenStore(bb, getLLVMType[type_decl], operand, GetNewRegOperand(vardecl_reg));
        }
    }
}

void BlockItem_Decl::codeIR() {
    // TODO("BlockItemDecl CodeIR");

    decl->codeIR();
}

void BlockItem_Stmt::codeIR() {
    // TODO("BlockItemStmt CodeIR");

    stmt->codeIR();
}

void __Block::codeIR() {
    // TODO("Block CodeIR");

    irgen_table.symbol_table.enter_scope();
    for (auto item : *item_list) {
        item->codeIR();
    }
    irgen_table.symbol_table.exit_scope();
}

void __FuncFParam::codeIR() { TODO("FunctionFParam CodeIR"); }

void __FuncDef::codeIR() {
    // TODO("FunctionDef CodeIR");

    // 成员变量
    // Type::ty return_type;
    // Symbol name;
    // std::vector<FuncFParam> *formals;
    // Block block;

    irgen_table.symbol_table.enter_scope();
    semant_table.symbol_table.enter_scope();

    // 清空函数块内信息
    cur_label = -1;
    cur_reg = -1;
    cur_cfg.func_cur_label = 0;
    irgen_table.reg_table.clear();
    return_label = 0;

    auto ret_type = getLLVMType[return_type];
    auto f_def = new FunctionDefineInstruction(ret_type, name->get_string());
    llvmIR.NewFunction(f_def);

    // 函数入口基本块，处理形参
    auto bb = llvmIR.NewBlock(f_def, NewLabel());

    cur_cfg.function_def = f_def;

    cur_reg = (*formals).size() - 1;
    for (int i = 0; i < (*formals).size(); i++) {
        auto formal = (*formals)[i];
        VarAttribute var;
        var.type = formal->type_decl;
        if (formal->dims) {    // 数组形参
            f_def->InsertFormal(getLLVMType[4]);
            auto formal_dims = *formal->dims;

            // 对于形参，忽略首个维度
            for (int i = 1; i < formal_dims.size(); i++) {
                var.dims.push_back(formal_dims[i]->attribute.V.val.IntVal);
            }

            // 数组形参不需要分配单独的寄存器，传递数组的首地址，即一个指向数组首元素的指针
            irgen_table.symbol_table.add_Symbol(formal->name, i);
            irgen_table.reg_table[i] = var;

        } else {    // 非数组形参
            f_def->InsertFormal(getLLVMType[formal->type_decl]);
            IRgenAlloca(bb, getLLVMType[formal->type_decl], NewReg());
            IRgenStore(bb, getLLVMType[formal->type_decl], GetNewRegOperand(i), GetNewRegOperand(cur_reg));
            // 非数组形参需要分配单独的寄存器
            irgen_table.symbol_table.add_Symbol(formal->name, cur_reg);
            irgen_table.reg_table[cur_reg] = var;
        }
    }

    IRgenBRUnCond(bb, cur_label + 1);

    // 函数体基本块，函数本身代码
    bb = llvmIR.NewBlock(f_def, NewLabel());

    cur_cfg.block_map = &llvmIR.function_block_map[f_def];
    cur_cfg.func_cur_label = cur_label;
    block->codeIR();

    // 对于标记的函数出口块，如果为空，或者最后的指令不是 RET/Br 类型，那么加入 RET 指令
    auto needRetBB = (*cur_cfg.block_map)[return_label];
    if (needRetBB->Instruction_list.empty() ||
        (needRetBB->Instruction_list.back()->GetOpcode() != BasicInstruction::LLVMIROpcode::RET &&
         needRetBB->Instruction_list.back()->GetOpcode() != BasicInstruction::LLVMIROpcode::BR_COND &&
         needRetBB->Instruction_list.back()->GetOpcode() != BasicInstruction::LLVMIROpcode::BR_UNCOND)) {
        if (return_type == Type::VOID) {
            IRgenRetVoid(needRetBB);
        } else if (return_type == Type::INT) {
            IRgenArithmeticI32ImmAll(needRetBB, BasicInstruction::LLVMIROpcode::ADD, 0, 0, NewReg());
            IRgenRetReg(needRetBB, BasicInstruction::LLVMType::I32, cur_reg);
        } else if (return_type == Type::FLOAT) {
            IRgenArithmeticF32ImmAll(needRetBB, BasicInstruction::LLVMIROpcode::FADD, 0, 0, NewReg());
            IRgenRetReg(needRetBB, BasicInstruction::LLVMType::FLOAT32, cur_reg);
        }
    }

    bool point2last = false;

    // 对于没有指令或者最后指令不是跳转、返回指令的基本块，添加无条件跳转语句
    for (auto pair : (*cur_cfg.block_map)) {
        if (pair.second->Instruction_list.empty()) {
            if (return_type == Type::VOID) {
                IRgenRetVoid(pair.second);
            } else if (return_type == Type::INT) {
                IRgenArithmeticI32ImmAll(pair.second, BasicInstruction::LLVMIROpcode::ADD, 0, 0, NewReg());
                IRgenRetReg(pair.second, BasicInstruction::LLVMType::I32, cur_reg);
            } else if (return_type == Type::FLOAT) {
                IRgenArithmeticF32ImmAll(pair.second, BasicInstruction::LLVMIROpcode::FADD, 0, 0, NewReg());
                IRgenRetReg(pair.second, BasicInstruction::LLVMType::FLOAT32, cur_reg);
            }
        } else {
            auto it = pair.second->Instruction_list.end();
            --it;
            auto ins = *it;
            if (!(ins->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_COND ||
                  ins->GetOpcode() == BasicInstruction::LLVMIROpcode::BR_UNCOND ||
                  ins->GetOpcode() == BasicInstruction::LLVMIROpcode::RET)) {
                IRgenBRUnCond(pair.second, pair.first + 1);
                if (pair.first == cur_label) {
                    point2last = true;
                }
            }
        }
    }

    // 如果当前基本块的最后指令是跳转到后一个基本块，则需要执行添加返回值操作
    if (point2last) {
        // 在函数体的末尾加返回语句，确保所有函数都有返回值
        auto retBB = llvmIR.NewBlock(cur_cfg.function_def, NewLabel());
        if (return_type == Type::VOID) {
            IRgenRetVoid(retBB);
        } else if (return_type == Type::INT) {
            IRgenArithmeticI32ImmAll(retBB, BasicInstruction::LLVMIROpcode::ADD, 0, 0, NewReg());
            IRgenRetReg(retBB, BasicInstruction::LLVMType::I32, cur_reg);
        } else if (return_type == Type::FLOAT) {
            IRgenArithmeticF32ImmAll(retBB, BasicInstruction::LLVMIROpcode::FADD, 0, 0, NewReg());
            IRgenRetReg(retBB, BasicInstruction::LLVMType::FLOAT32, cur_reg);
        }
    }

    llvmIR.def_reg[f_def] = cur_reg;

    irgen_table.symbol_table.exit_scope();
    semant_table.symbol_table.exit_scope();
}

void CompUnit_Decl::codeIR() {
    // TODO("CompUnitDecl CodeIR");

    decl->codeIR();
}

void CompUnit_FuncDef::codeIR() { func_def->codeIR(); }

void AddLibFunctionDeclare() {
    FunctionDeclareInstruction *getint = new FunctionDeclareInstruction(BasicInstruction::I32, "getint");
    llvmIR.function_declare.push_back(getint);

    FunctionDeclareInstruction *getchar = new FunctionDeclareInstruction(BasicInstruction::I32, "getch");
    llvmIR.function_declare.push_back(getchar);

    FunctionDeclareInstruction *getfloat = new FunctionDeclareInstruction(BasicInstruction::FLOAT32, "getfloat");
    llvmIR.function_declare.push_back(getfloat);

    FunctionDeclareInstruction *getarray = new FunctionDeclareInstruction(BasicInstruction::I32, "getarray");
    getarray->InsertFormal(BasicInstruction::PTR);
    llvmIR.function_declare.push_back(getarray);

    FunctionDeclareInstruction *getfloatarray = new FunctionDeclareInstruction(BasicInstruction::I32, "getfarray");
    getfloatarray->InsertFormal(BasicInstruction::PTR);
    llvmIR.function_declare.push_back(getfloatarray);

    FunctionDeclareInstruction *putint = new FunctionDeclareInstruction(BasicInstruction::VOID, "putint");
    putint->InsertFormal(BasicInstruction::I32);
    llvmIR.function_declare.push_back(putint);

    FunctionDeclareInstruction *putch = new FunctionDeclareInstruction(BasicInstruction::VOID, "putch");
    putch->InsertFormal(BasicInstruction::I32);
    llvmIR.function_declare.push_back(putch);

    FunctionDeclareInstruction *putfloat = new FunctionDeclareInstruction(BasicInstruction::VOID, "putfloat");
    putfloat->InsertFormal(BasicInstruction::FLOAT32);
    llvmIR.function_declare.push_back(putfloat);

    FunctionDeclareInstruction *putarray = new FunctionDeclareInstruction(BasicInstruction::VOID, "putarray");
    putarray->InsertFormal(BasicInstruction::I32);
    putarray->InsertFormal(BasicInstruction::PTR);
    llvmIR.function_declare.push_back(putarray);

    FunctionDeclareInstruction *putfarray = new FunctionDeclareInstruction(BasicInstruction::VOID, "putfarray");
    putfarray->InsertFormal(BasicInstruction::I32);
    putfarray->InsertFormal(BasicInstruction::PTR);
    llvmIR.function_declare.push_back(putfarray);

    FunctionDeclareInstruction *starttime = new FunctionDeclareInstruction(BasicInstruction::VOID, "_sysy_starttime");
    starttime->InsertFormal(BasicInstruction::I32);
    llvmIR.function_declare.push_back(starttime);

    FunctionDeclareInstruction *stoptime = new FunctionDeclareInstruction(BasicInstruction::VOID, "_sysy_stoptime");
    stoptime->InsertFormal(BasicInstruction::I32);
    llvmIR.function_declare.push_back(stoptime);

    // 一些llvm自带的函数，也许会为你的优化提供帮助
    FunctionDeclareInstruction *llvm_memset =
    new FunctionDeclareInstruction(BasicInstruction::VOID, "llvm.memset.p0.i32");
    llvm_memset->InsertFormal(BasicInstruction::PTR);
    llvm_memset->InsertFormal(BasicInstruction::I8);
    llvm_memset->InsertFormal(BasicInstruction::I32);
    llvm_memset->InsertFormal(BasicInstruction::I1);
    llvmIR.function_declare.push_back(llvm_memset);

    FunctionDeclareInstruction *llvm_umax = new FunctionDeclareInstruction(BasicInstruction::I32, "llvm.umax.i32");
    llvm_umax->InsertFormal(BasicInstruction::I32);
    llvm_umax->InsertFormal(BasicInstruction::I32);
    llvmIR.function_declare.push_back(llvm_umax);

    FunctionDeclareInstruction *llvm_umin = new FunctionDeclareInstruction(BasicInstruction::I32, "llvm.umin.i32");
    llvm_umin->InsertFormal(BasicInstruction::I32);
    llvm_umin->InsertFormal(BasicInstruction::I32);
    llvmIR.function_declare.push_back(llvm_umin);

    FunctionDeclareInstruction *llvm_smax = new FunctionDeclareInstruction(BasicInstruction::I32, "llvm.smax.i32");
    llvm_smax->InsertFormal(BasicInstruction::I32);
    llvm_smax->InsertFormal(BasicInstruction::I32);
    llvmIR.function_declare.push_back(llvm_smax);

    FunctionDeclareInstruction *llvm_smin = new FunctionDeclareInstruction(BasicInstruction::I32, "llvm.smin.i32");
    llvm_smin->InsertFormal(BasicInstruction::I32);
    llvm_smin->InsertFormal(BasicInstruction::I32);
    llvmIR.function_declare.push_back(llvm_smin);
}
