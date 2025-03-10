#include "riscv64_instSelect.h"
#include <sstream>
#include <utility>
#include <vector>

// 返回加载 immf 的 FLOAT64 寄存器
Register RiscV64Selector::fromImmF(float immf) {
    Register tmp = cur_func->GetNewReg(INT64);

    // lui t0, %hi(1066192077)  # 加载高20位
    // addi t0, t0, %lo(1066192077)  # 加载低12位
    unsigned int *p = reinterpret_cast<unsigned int *>(&immf);
    auto binRep = *p;                          // 浮点数的二进制表示
    auto high20 = (binRep >> 12) & 0xFFFFF;    // 提取高20位
    auto low12 = binRep & 0xFFF;               // 提取低12位

    auto lui_inst = rvconstructor->ConstructUImm(RISCV_LUI, tmp, high20);
    cur_block->push_back(lui_inst);
    while (low12 > 2047) {
        // addi 立即数范围是 -2048 到 2047
        auto addi_inst = rvconstructor->ConstructIImm(RISCV_ADDI, tmp, tmp, 2047);
        cur_block->push_back(addi_inst);
        low12 -= 2047;
    }
    auto addi_inst = rvconstructor->ConstructIImm(RISCV_ADDI, tmp, tmp, low12);
    cur_block->push_back(addi_inst);

    auto fp_reg = cur_func->GetNewReg(FLOAT64);
    auto s2fp_inst = rvconstructor->ConstructR2(RISCV_FMV_W_X, fp_reg, tmp);
    cur_block->push_back(s2fp_inst);
    return fp_reg;
}

// 返回加载 imm 的 INT64 寄存器
Register RiscV64Selector::fromImm(int imm) {
    Register tmp = cur_func->GetNewReg(INT64);

    // 提取高 20 位和低 12 位
    auto high20 = (imm >> 12) & 0xFFFFF;    // 处理符号扩展，四舍五入到高位
    auto low12 = imm & 0xFFF;               // 提取低 12 位

    if (high20 != 0) {
        // 如果高 20 位非零，生成 lui 指令加载高 20 位
        auto lui_inst = rvconstructor->ConstructUImm(RISCV_LUI, tmp, high20);
        cur_block->push_back(lui_inst);
    } else {
        // 如果高 20 位为零，直接从 x0 初始化 tmp 寄存器为零
        auto zero_inst = rvconstructor->ConstructIImm(RISCV_ADDI, tmp, GetPhysicalReg(RISCV_x0), 0);
        cur_block->push_back(zero_inst);
    }

    if (low12 != 0) {
        // 如果低 12 位非零，生成 addi 指令加载低 12 位
        while (low12 > 2047) {
            // addi 立即数范围是 -2048 到 2047
            auto addi_inst = rvconstructor->ConstructIImm(RISCV_ADDI, tmp, tmp, 2047);
            cur_block->push_back(addi_inst);
            low12 -= 2047;
        }
        auto addi_inst = rvconstructor->ConstructIImm(RISCV_ADDI, tmp, tmp, low12);
        cur_block->push_back(addi_inst);
    }
    return tmp;
}

// Reference
// https://gitlab.eduxiji.net/educg-group-26173-2487151/T202410055203113-826/-/blob/master/target/riscv64gc/instruction_select/riscv64_instSelect.cc:1064-1100
// 返回比较关系，是否交换操作数顺序和置位比较关系
int FcmpCond_2_op(BasicInstruction::FcmpCond x, bool &reverse, int &riscv_opcode) {
    int opcode;
    reverse = false;
    switch (x) {
    case BasicInstruction::OEQ:
    case BasicInstruction::UEQ:
        opcode = RISCV_BNE;
        riscv_opcode = RISCV_FEQ_S;
        break;
    case BasicInstruction::OGT:
    case BasicInstruction::UGT:
        opcode = RISCV_BNE;
        riscv_opcode = RISCV_FLT_S;
        reverse = true;
        break;
    case BasicInstruction::OGE:
    case BasicInstruction::UGE:
        opcode = RISCV_BNE;
        riscv_opcode = RISCV_FLE_S;
        reverse = true;
        break;
    case BasicInstruction::OLT:
    case BasicInstruction::ULT:
        opcode = RISCV_BNE;
        riscv_opcode = RISCV_FLT_S;
        break;
    case BasicInstruction::OLE:
    case BasicInstruction::ULE:
        opcode = RISCV_BNE;
        riscv_opcode = RISCV_FLE_S;
        break;
    case BasicInstruction::ONE:
    case BasicInstruction::UNE:
        // 当两个操作数不相等时，使用 FEQ 指令得到的 set_result = 0, 然后使用 beq set_result, 0 即可完成跳转
        opcode = RISCV_BEQ;
        riscv_opcode = RISCV_FEQ_S;
        break;
    case BasicInstruction::ORD:
    case BasicInstruction::UNO:
    case BasicInstruction::TRUE:
    case BasicInstruction::FALSE:
    default:
        TODO("...");
    }
    return opcode;
}

// 返回比较关系
int IcmpCond_2_Op(BasicInstruction::IcmpCond x) {
    int op;
    switch (x) {
    case BasicInstruction::eq:
        op = RISCV_BEQ;
        break;
    case BasicInstruction::ne:
        op = RISCV_BNE;
        break;
    case BasicInstruction::ugt:
        op = RISCV_BGTU;
        break;
    case BasicInstruction::uge:
        op = RISCV_BGEU;
        break;
    case BasicInstruction::ult:
        op = RISCV_BLTU;
        break;
    case BasicInstruction::ule:
        op = RISCV_BLEU;
        break;
    case BasicInstruction::sgt:
        op = RISCV_BGT;
        break;
    case BasicInstruction::sge:
        op = RISCV_BGE;
        break;
    case BasicInstruction::slt:
        op = RISCV_BLT;
        break;
    case BasicInstruction::sle:
        op = RISCV_BLE;
        break;
    }
    return op;
}

template <> void RiscV64Selector::ConvertAndAppend<LoadInstruction *>(LoadInstruction *ins) {
    // TODO("Implement this if you need");

    // lui r1, %hi(x)
    // lw  r2, %lo(x)(r1)
    if (ins->GetPointer()->GetOperandType() == BasicOperand::GLOBAL) {
        auto global = (GlobalOperand *)ins->GetPointer();
        auto result = (RegOperand *)ins->GetResult();
        Register r1 = cur_func->GetNewReg(INT64);
        if (ins->GetDataType() == BasicInstruction::LLVMType::I32) {
            Register r2(true, result->GetRegNo(), INT64, false);
            auto lui_inst = rvconstructor->ConstructULabel(RISCV_LUI, r1, RiscVLabel(global->GetName(), true));
            auto lw_inst = rvconstructor->ConstructILabel(RISCV_LW, r2, r1, RiscVLabel(global->GetName(), false));
            cur_block->push_back(lui_inst);
            cur_block->push_back(lw_inst);
        } else if (ins->GetDataType() == BasicInstruction::FLOAT32) {
            Register r2(true, result->GetRegNo(), FLOAT64, false);
            auto lui_inst = rvconstructor->ConstructULabel(RISCV_LUI, r1, RiscVLabel(global->GetName(), true));
            auto lw_inst = rvconstructor->ConstructILabel(RISCV_FLW, r2, r1, RiscVLabel(global->GetName(), false));
            cur_block->push_back(lui_inst);
            cur_block->push_back(lw_inst);
        } else {
            TODO("...");
        }
    } else if (ins->GetPointer()->GetOperandType() == BasicOperand::REG) {
        // TODO("指向寄存器的指针");
        auto rd_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
        auto ptr_op = (RegOperand *)ins->GetPointer();
        if (ins->GetDataType() == BasicInstruction::I32) {
            Register rd(true, rd_reg, INT64, false);
            if (alloca_off.find(ptr_op) != alloca_off.end()) {
                // 已经在 sp 中分配空间的寄存器：从 sp 中加载
                auto offset = alloca_off[ptr_op];
                auto lw_inst = rvconstructor->ConstructIImm(RISCV_LW, rd, GetPhysicalReg(RISCV_sp), offset);
                cur_block->push_back(lw_inst);
            } else {
                // TODO("未记录alloca的寄存器");
                // 未分配空间的：直接从 ptr 寄存器中加载
                Register tmp(true, ptr_op->GetRegNo(), INT64, false);
                auto lw_inst = rvconstructor->ConstructIImm(RISCV_LW, rd, tmp, 0);
                cur_block->push_back(lw_inst);
            }
        } else if (ins->GetDataType() == BasicInstruction::FLOAT32) {
            // TODO("其他类型实现");
            Register rd(true, rd_reg, FLOAT64, false);
            if (alloca_off.find(ptr_op) != alloca_off.end()) {
                auto offset = alloca_off[ptr_op];
                auto lw_inst = rvconstructor->ConstructIImm(RISCV_FLW, rd, GetPhysicalReg(RISCV_sp), offset);
                cur_block->push_back(lw_inst);
            } else {
                // TODO("未记录alloca的寄存器");
                Register tmp(true, ptr_op->GetRegNo(), INT64, false);
                auto lw_inst = rvconstructor->ConstructIImm(RISCV_FLW, rd, tmp, 0);
                cur_block->push_back(lw_inst);
            }
        } else {
            TODO("...");
        }
    }
}

template <> void RiscV64Selector::ConvertAndAppend<StoreInstruction *>(StoreInstruction *ins) {
    // TODO("Implement this if you need");

    Register val;
    if (ins->GetValue()->GetOperandType() == BasicOperand::REG) {
        auto val_reg = ((RegOperand *)ins->GetValue())->GetRegNo();
        if (ins->GetDataType() == BasicInstruction::I32) {
            val = Register(true, val_reg, INT64, false);
        } else if (ins->GetDataType() == BasicInstruction::FLOAT32) {
            val = Register(true, val_reg, FLOAT64, false);
            // TODO("REG其他类型实现");
        }
    } else {
        TODO("Store指令其他类型value实现");
    }

    if (ins->GetPointer()->GetOperandType() == BasicOperand::GLOBAL) {
        /*
            lui			tmp,%hi(a)
            sw			t0,%lo(a)(tmp)
        */
        auto tmp = cur_func->GetNewReg(INT64);
        auto global = (GlobalOperand *)ins->GetPointer();
        auto lui_inst = rvconstructor->ConstructULabel(RISCV_LUI, tmp, RiscVLabel(global->GetName(), true));
        cur_block->push_back(lui_inst);
        if (ins->GetDataType() == BasicInstruction::I32) {
            auto sw_inst = rvconstructor->ConstructSLabel(RISCV_SW, val, tmp, RiscVLabel(global->GetName(), false));
            cur_block->push_back(sw_inst);
        } else {
            TODO("其他Store指令类型实现");
        }
    } else if (ins->GetPointer()->GetOperandType() == BasicOperand::REG) {
        // TODO("Store指令其他类型指针实现");
        auto ptr_op = (RegOperand *)ins->GetPointer();
        if (ins->GetDataType() == BasicInstruction::I32) {
            if (alloca_off.find(ptr_op) != alloca_off.end()) {
                // 已经记录在 sp 中的变量：保存结果到 sp 中
                auto offset = alloca_off[ptr_op];
                auto sw_inst = rvconstructor->ConstructSImm(RISCV_SW, val, GetPhysicalReg(RISCV_sp), offset);
                cur_block->push_back(sw_inst);
            } else {
                // TODO("向没alloca的变量Store");
                Register ptr(true, ptr_op->GetRegNo(), INT64, false);
                auto sw_inst = rvconstructor->ConstructSImm(RISCV_SW, val, ptr, 0);
                cur_block->push_back(sw_inst);
            }
        } else if (ins->GetDataType() == BasicInstruction::FLOAT32) {
            // TODO("...");
            if (alloca_off.find(ptr_op) != alloca_off.end()) {
                auto offset = alloca_off[ptr_op];
                auto sw_inst = rvconstructor->ConstructSImm(RISCV_FSW, val, GetPhysicalReg(RISCV_sp), offset);
                cur_block->push_back(sw_inst);
            } else {
                // TODO("向没alloca的寄存器Store");
                Register ptr(true, ptr_op->GetRegNo(), INT64, false);
                auto sw_inst = rvconstructor->ConstructSImm(RISCV_FSW, val, ptr, 0);
                cur_block->push_back(sw_inst);
            }
        }
    }
}

template <> void RiscV64Selector::ConvertAndAppend<ArithmeticInstruction *>(ArithmeticInstruction *ins) {
    // TODO("Implement this if you need");
    switch (ins->GetOpcode()) {
    case BasicInstruction::LLVMIROpcode::ADD:
        if (ins->GetDataType() == BasicInstruction::LLVMType::I32) {
            if (ins->GetOperand1()->GetOperandType() == BasicOperand::IMMI32) {
                int imm_val1 = ((ImmI32Operand *)ins->GetOperand1())->GetIntImmVal();

                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMI32) {
                    // 立即数 + 立即数：分解为 addiw rd, x0, (imm1 + imm2)
                    int imm_val2 = ((ImmI32Operand *)ins->GetOperand2())->GetIntImmVal();
                    int imm_sum = imm_val1 + imm_val2;

                    if (imm_sum > 2047) {
                        Register tmp = fromImm(imm_sum);
                        auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                        Register dest(true, res_reg, INT64, false);
                        auto inst = rvconstructor->ConstructR(RISCV_ADDW, dest, GetPhysicalReg(RISCV_x0), tmp);
                        cur_block->push_back(inst);
                    } else if (imm_sum < -2048) {
                        Register tmp = fromImm(-imm_sum);
                        auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                        Register dest(true, res_reg, INT64, false);
                        auto inst = rvconstructor->ConstructR(RISCV_SUBW, dest, GetPhysicalReg(RISCV_x0), tmp);
                        cur_block->push_back(inst);
                    } else {
                        auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                        Register dest(true, res_reg, INT64, false);
                        auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, dest, GetPhysicalReg(RISCV_x0), imm_sum);
                        cur_block->push_back(inst);
                    }
                } else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    // 立即数 + REG：addiw rd, rs2, imm
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    Register rs2(true, rs2_reg, INT64, false);
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register dest(true, res_reg, INT64, false);

                    if (imm_val1 > 2047) {
                        Register tmp = fromImm(imm_val1);
                        auto add_inst = rvconstructor->ConstructR(RISCV_ADDW, dest, tmp, rs2);
                        cur_block->push_back(add_inst);
                    } else if (imm_val1 < -2048) {
                        Register tmp = fromImm(-imm_val1);
                        auto add_inst = rvconstructor->ConstructR(RISCV_SUBW, dest, rs2, tmp);
                        cur_block->push_back(add_inst);
                    } else {
                        auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, dest, rs2, imm_val1);
                        cur_block->push_back(inst);
                    }
                }
            } else if (ins->GetOperand1()->GetOperandType() == BasicOperand::REG) {
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMI32) {
                    // REG + 立即数：addiw rd, rs1, imm2
                    int imm_val2 = ((ImmI32Operand *)ins->GetOperand2())->GetIntImmVal();
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rs1(true, rs1_reg, INT64, false);
                    Register dest(true, res_reg, INT64, false);

                    if (imm_val2 > 2047) {
                        Register tmp = fromImm(imm_val2);
                        auto add_inst = rvconstructor->ConstructR(RISCV_ADDW, dest, rs1, tmp);
                        cur_block->push_back(add_inst);
                    } else if (imm_val2 < -2048) {
                        Register tmp = fromImm(-imm_val2);
                        auto add_inst = rvconstructor->ConstructR(RISCV_SUBW, dest, rs1, tmp);
                        cur_block->push_back(add_inst);
                    } else {
                        auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, dest, rs1, imm_val2);
                        cur_block->push_back(inst);
                    }
                } else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    // REG + REG：addw rd, rs1, rs2
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rs1(true, rs1_reg, INT64, false);
                    Register rs2(true, rs2_reg, INT64, false);
                    Register dest(true, res_reg, INT64, false);
                    auto inst = rvconstructor->ConstructR(RISCV_ADDW, dest, rs1, rs2);
                    cur_block->push_back(inst);
                }
            }
        }
        break;
    case BasicInstruction::LLVMIROpcode::SUB:
        if (ins->GetDataType() == BasicInstruction::LLVMType::I32) {
            if (ins->GetOperand1()->GetOperandType() == BasicOperand::IMMI32) {
                int imm_val1 = ((ImmI32Operand *)ins->GetOperand1())->GetIntImmVal();
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMI32) {
                    TODO("imm - imm");

                } else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    // 立即数 - REG : subw rs, reg(imm1), reg
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    Register rs2(true, rs2_reg, INT64, false);
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register dest(true, res_reg, INT64, false);

                    auto tmp = fromImm(imm_val1);
                    auto add_inst = rvconstructor->ConstructR(RISCV_SUBW, dest, tmp, rs2);
                    cur_block->push_back(add_inst);
                }
            } else if (ins->GetOperand1()->GetOperandType() == BasicOperand::REG) {
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMI32) {
                    TODO("reg - imm");
                    /* REG + 立即数：addiw rd, rs1, imm2
                    int imm_val2 = ((ImmI32Operand *)ins->GetOperand2())->GetIntImmVal();
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rs1(true, rs1_reg, INT64, false);
                    Register dest(true, res_reg, INT64, false);

                    if (imm_val2 > 2047) {
                        Register tmp = fromImm(imm_val2);
                        auto add_inst = rvconstructor->ConstructR(RISCV_ADDW, dest, rs1, tmp);
                        cur_block->push_back(add_inst);
                    } else if (imm_val2 < -2048) {
                        Register tmp = fromImm(-imm_val2);
                        auto add_inst = rvconstructor->ConstructR(RISCV_SUBW, dest, rs1, tmp);
                        cur_block->push_back(add_inst);
                    } else {
                        auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, dest, rs1, imm_val2);
                        cur_block->push_back(inst);
                    }
                    */
                } else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    // REG - REG：addw rd, rs1, rs2
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rs1(true, rs1_reg, INT64, false);
                    Register rs2(true, rs2_reg, INT64, false);
                    Register dest(true, res_reg, INT64, false);
                    auto inst = rvconstructor->ConstructR(RISCV_SUBW, dest, rs1, rs2);
                    cur_block->push_back(inst);
                }
            }
        }
        break;
    case BasicInstruction::LLVMIROpcode::MUL:
        if (ins->GetDataType() == BasicInstruction::I32) {
            Register rs1, rs2;
            if (ins->GetOperand1()->GetOperandType() == BasicOperand::REG) {
                auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                rs1 = Register(true, rs1_reg, INT64, false);
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    rs2 = Register(true, rs2_reg, INT64, false);

                    auto rd_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rd(true, rd_reg, INT64, false);

                    auto mul_inst = rvconstructor->ConstructR(RISCV_MULW, rd, rs1, rs2);
                    cur_block->push_back(mul_inst);
                } else {
                    TODO("...");
                }
            } else {
                TODO("其他类型实现");
            }
        } else {
            TODO("浮点数实现");
        }
        break;

    case BasicInstruction::LLVMIROpcode::MOD:
        if (ins->GetDataType() == BasicInstruction::I32) {
            Register rs1, rs2;
            if (ins->GetOperand1()->GetOperandType() == BasicOperand::REG) {
                auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                rs1 = Register(true, rs1_reg, INT64, false);
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    rs2 = Register(true, rs2_reg, INT64, false);

                    auto rd_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rd(true, rd_reg, INT64, false);

                    auto mod_inst = rvconstructor->ConstructR(RISCV_REMW, rd, rs1, rs2);
                    cur_block->push_back(mod_inst);
                } else {
                    TODO("...");
                }
            } else {
                TODO("其他类型实现");
            }
        } else {
            TODO("浮点数实现");
        }
        break;
    case BasicInstruction::LLVMIROpcode::DIV:
        if (ins->GetDataType() == BasicInstruction::I32) {
            Register rs1, rs2;
            if (ins->GetOperand1()->GetOperandType() == BasicOperand::REG) {
                auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                rs1 = Register(true, rs1_reg, INT64, false);
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    rs2 = Register(true, rs2_reg, INT64, false);

                    auto rd_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rd(true, rd_reg, INT64, false);

                    auto div_inst = rvconstructor->ConstructR(RISCV_DIVW, rd, rs1, rs2);
                    cur_block->push_back(div_inst);
                } else {
                    TODO("...");
                }
            } else {
                TODO("其他类型实现");
            }
        } else {
            TODO("浮点数实现");
        }
        break;

    case BasicInstruction::LLVMIROpcode::FADD:
        if (ins->GetDataType() == BasicInstruction::LLVMType::FLOAT32) {
            if (ins->GetOperand1()->GetOperandType() == BasicOperand::IMMF32) {
                // 立即数 + 立即数
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMF32) {
                    // TODO("11");
                    auto imm_val1 = ((ImmF32Operand *)ins->GetOperand1())->GetFloatVal();
                    auto imm_val2 = ((ImmF32Operand *)ins->GetOperand2())->GetFloatVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();

                    auto imm = imm_val1 + imm_val2;
                    auto value = fromImmF(imm);
                    auto zero = fromImmF(0);
                    Register res(true, res_reg, FLOAT64, false);
                    auto inst = rvconstructor->ConstructR(RISCV_FADD_S, res, value, zero);
                    cur_block->push_back(inst);
                }
                // 立即数 + REG
                else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    TODO("12");
                    /*
                     auto imm_val = ((ImmI32Operand *)ins->GetOperand1())->GetIntImmVal();
                     auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                     auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                     Register rs2(true, rs2_reg, INT64, false);
                     Register res(true, res_reg, INT64, false);
                     auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, res, rs2, imm_val);
                     cur_block->push_back(inst);
                     */
                }

            } else if (ins->GetOperand1()->GetOperandType() == BasicOperand::REG) {
                // REG + 立即数
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMI32) {
                    TODO("13");
                    /*
                    auto imm_val2 = ((ImmI32Operand *)ins->GetOperand2())->GetIntImmVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    Register rs1(true, rs1_reg, INT64, false);
                    Register res(true, res_reg, INT64, false);
                    auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, res, rs1, imm_val2);
                    cur_block->push_back(inst);
                    */
                }
                // REG + REG：fadds rd, rs1, rs2
                else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rs1(true, rs1_reg, FLOAT64, false);
                    Register rs2(true, rs2_reg, FLOAT64, false);
                    Register res(true, res_reg, FLOAT64, false);
                    auto inst = rvconstructor->ConstructR(RISCV_FADD_S, res, rs1, rs2);
                    cur_block->push_back(inst);
                }
            }
        }
        break;

    case BasicInstruction::LLVMIROpcode::FSUB:
        if (ins->GetDataType() == BasicInstruction::LLVMType::FLOAT32) {
            if (ins->GetOperand1()->GetOperandType() == BasicOperand::IMMF32) {
                // 立即数 + 立即数：addiw rd, x0, imm1+imm2
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMF32) {
                    TODO("...");
                    auto imm_val1 = ((ImmF32Operand *)ins->GetOperand1())->GetFloatVal();
                    auto imm_val2 = ((ImmF32Operand *)ins->GetOperand2())->GetFloatVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register dest(true, res_reg, FLOAT64, false);

                    auto imm = imm_val1 + imm_val2;
                    dest = fromImmF(imm);

                }
                // 立即数 - REG
                else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    // TODO("12");

                    auto imm_val = ((ImmF32Operand *)ins->GetOperand1())->GetFloatVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    Register rs2(true, rs2_reg, FLOAT64, false);
                    Register res(true, res_reg, FLOAT64, false);
                    Register rs1 = fromImmF(imm_val);
                    auto inst = rvconstructor->ConstructR(RISCV_FSUB_S, res, rs1, rs2);
                    cur_block->push_back(inst);
                }

            } else if (ins->GetOperand1()->GetOperandType() == BasicOperand::REG) {
                // REG - 立即数
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMI32) {
                    TODO("13");
                    auto imm_val2 = ((ImmI32Operand *)ins->GetOperand2())->GetIntImmVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    Register rs1(true, rs1_reg, INT64, false);
                    Register res(true, res_reg, INT64, false);
                    auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, res, rs1, imm_val2);
                    cur_block->push_back(inst);
                }
                // REG - REG：fadds rd, rs1, rs2
                else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rs1(true, rs1_reg, FLOAT64, false);
                    Register rs2(true, rs2_reg, FLOAT64, false);
                    Register res(true, res_reg, FLOAT64, false);
                    auto inst = rvconstructor->ConstructR(RISCV_FSUB_S, res, rs1, rs2);
                    cur_block->push_back(inst);
                }
            }
        }
        break;
    case BasicInstruction::LLVMIROpcode::FMUL:
        if (ins->GetDataType() == BasicInstruction::LLVMType::FLOAT32) {
            if (ins->GetOperand1()->GetOperandType() == BasicOperand::IMMF32) {
                // 立即数 + 立即数：addiw rd, x0, imm1+imm2
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMF32) {
                    TODO("...");
                    /*
                    auto imm_val1 = ((ImmF32Operand *)ins->GetOperand1())->GetFloatVal();
                    auto imm_val2 = ((ImmF32Operand *)ins->GetOperand2())->GetFloatVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register dest(true, res_reg, FLOAT64, false);

                    auto imm = imm_val1 + imm_val2;
                    auto imm_reg = fromImmF(imm);
                    auto zero = fromImmF(0);
                    auto move_inst = rvconstructor->ConstructR(RISCV_FADD_S, dest, zero, imm_reg);
                    cur_block->push_back(move_inst);
                    */
                }
                // 立即数 - REG：addiw rd, rs2, imm
                else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    TODO("12");
                    auto imm_val = ((ImmI32Operand *)ins->GetOperand1())->GetIntImmVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    Register rs2(true, rs2_reg, INT64, false);
                    Register res(true, rs2_reg, INT64, false);
                    auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, res, rs2, imm_val);
                    cur_block->push_back(inst);
                }

            } else if (ins->GetOperand1()->GetOperandType() == BasicOperand::REG) {
                // REG - 立即数
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMI32) {
                    TODO("13");
                    auto imm_val2 = ((ImmI32Operand *)ins->GetOperand2())->GetIntImmVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    Register rs1(true, rs1_reg, INT64, false);
                    Register res(true, res_reg, INT64, false);
                    auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, res, rs1, imm_val2);
                    cur_block->push_back(inst);
                }
                // REG - REG：fadds rd, rs1, rs2
                else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rs1(true, rs1_reg, FLOAT64, false);
                    Register rs2(true, rs2_reg, FLOAT64, false);
                    Register res(true, res_reg, FLOAT64, false);
                    auto inst = rvconstructor->ConstructR(RISCV_FMUL_S, res, rs1, rs2);
                    cur_block->push_back(inst);
                }
            }
        }
        break;
    case BasicInstruction::FDIV:
        if (ins->GetDataType() == BasicInstruction::LLVMType::FLOAT32) {
            if (ins->GetOperand1()->GetOperandType() == BasicOperand::IMMF32) {
                TODO("...");
                // 立即数 + 立即数：addiw rd, x0, imm1+imm2
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMF32) {
                    auto imm_val1 = ((ImmF32Operand *)ins->GetOperand1())->GetFloatVal();
                    auto imm_val2 = ((ImmF32Operand *)ins->GetOperand2())->GetFloatVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();

                    auto imm = imm_val1 + imm_val2;
                    auto dest = fromImmF(imm);
                }
                // 立即数 + REG：addiw rd, rs2, imm
                else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    TODO("12");
                    auto imm_val = ((ImmI32Operand *)ins->GetOperand1())->GetIntImmVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    Register rs2(true, rs2_reg, INT64, false);
                    Register res(true, rs2_reg, INT64, false);
                    auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, res, rs2, imm_val);
                    cur_block->push_back(inst);
                }

            } else if (ins->GetOperand1()->GetOperandType() == BasicOperand::REG) {
                // REG + 立即数
                if (ins->GetOperand2()->GetOperandType() == BasicOperand::IMMI32) {
                    TODO("13");
                    auto imm_val2 = ((ImmI32Operand *)ins->GetOperand2())->GetIntImmVal();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    Register rs1(true, rs1_reg, INT64, false);
                    Register res(true, res_reg, INT64, false);
                    auto inst = rvconstructor->ConstructIImm(RISCV_ADDIW, res, rs1, imm_val2);
                    cur_block->push_back(inst);
                }
                // REG + REG：fadds rd, rs1, rs2
                else if (ins->GetOperand2()->GetOperandType() == BasicOperand::REG) {
                    auto rs1_reg = ((RegOperand *)ins->GetOperand1())->GetRegNo();
                    auto rs2_reg = ((RegOperand *)ins->GetOperand2())->GetRegNo();
                    auto res_reg = ((RegOperand *)ins->GetResult())->GetRegNo();
                    Register rs1(true, rs1_reg, FLOAT64, false);
                    Register rs2(true, rs2_reg, FLOAT64, false);
                    Register res(true, res_reg, FLOAT64, false);
                    auto inst = rvconstructor->ConstructR(RISCV_FDIV_S, res, rs1, rs2);
                    cur_block->push_back(inst);
                }
            }
        }
        break;

    default:

        TODO("算数未完待续...");
    }
}

template <> void RiscV64Selector::ConvertAndAppend<IcmpInstruction *>(IcmpInstruction *ins) {
    // TODO("Implement this if you need");
    auto rs = ((RegOperand *)ins->GetResult());
    cmp_map[rs] = ins;
}

template <> void RiscV64Selector::ConvertAndAppend<FcmpInstruction *>(FcmpInstruction *ins) {
    // TODO("Implement this if you need");
    auto rs = ((RegOperand *)ins->GetResult());
    cmp_map[rs] = ins;
}

template <> void RiscV64Selector::ConvertAndAppend<AllocaInstruction *>(AllocaInstruction *ins) {
    // TODO("Implement this if you need");
    auto dims = ins->GetDims();
    int alloca_size = 1;
    for (auto d : dims) {
        alloca_size *= d;
    }
    alloca_size *= 4;
    // 记录 var-offset
    alloca_off[(RegOperand *)ins->GetResult()] = cur_offset;
    cur_offset += alloca_size;
}

template <> void RiscV64Selector::ConvertAndAppend<BrCondInstruction *>(BrCondInstruction *ins) {
    auto cond_op = (RegOperand *)ins->GetCond();

    // 获取保存的跳转语句
    auto cmp_ins = cmp_map[cond_op];
    int opcode;

    Register cmp_op1, cmp_op2;
    if (cmp_ins->GetOpcode() == BasicInstruction::ICMP) {
        auto icmp_ins = (IcmpInstruction *)cmp_ins;
        if (icmp_ins->GetOp1()->GetOperandType() == BasicOperand::REG) {
            cmp_op1 = Register(true, ((RegOperand *)icmp_ins->GetOp1())->GetRegNo(), INT64);
        } else if (icmp_ins->GetOp1()->GetOperandType() == BasicOperand::IMMI32) {
            if (((ImmI32Operand *)icmp_ins->GetOp1())->GetIntImmVal() != 0) {
                cmp_op1 = fromImm(((ImmI32Operand *)icmp_ins->GetOp1())->GetIntImmVal());
            } else {
                cmp_op1 = GetPhysicalReg(RISCV_x0);
            }
        }
        if (icmp_ins->GetOp2()->GetOperandType() == BasicOperand::REG) {
            cmp_op2 = Register(true, ((RegOperand *)icmp_ins->GetOp2())->GetRegNo(), INT64);
        } else if (icmp_ins->GetOp2()->GetOperandType() == BasicOperand::IMMI32) {
            if (((ImmI32Operand *)icmp_ins->GetOp2())->GetIntImmVal() != 0) {
                cmp_op2 = fromImm(((ImmI32Operand *)icmp_ins->GetOp2())->GetIntImmVal());
            } else {
                cmp_op2 = GetPhysicalReg(RISCV_x0);
            }
        } else {
            ERROR("Unexpected ICMP op2 type");
        }
        opcode = IcmpCond_2_Op(icmp_ins->GetCond());
    } else if (cmp_ins->GetOpcode() == BasicInstruction::FCMP) {
        // TODO("...");
        // 浮点数无法直接使用 b(cond) 指令进行判断关系并跳转，需要借助浮点比较指令对某一寄存器进行置位，然后将置位结果和
        // 0 进行比较完成条件跳转

        // 保存置位结果的寄存器
        Register set_result = cur_func->GetNewReg(INT64);

        // 对两个操作数进行处理
        auto fcmp_ins = (FcmpInstruction *)cmp_ins;
        if (fcmp_ins->GetOp1()->GetOperandType() == BasicOperand::REG) {
            cmp_op1 = Register(true, ((RegOperand *)fcmp_ins->GetOp1())->GetRegNo(), FLOAT64);
        } else if (fcmp_ins->GetOp1()->GetOperandType() == BasicOperand::IMMF32) {
            cmp_op1 = fromImmF(((ImmF32Operand *)fcmp_ins->GetOp1())->GetFloatVal());
        }
        if (fcmp_ins->GetOp2()->GetOperandType() == BasicOperand::REG) {
            cmp_op2 = Register(true, ((RegOperand *)fcmp_ins->GetOp2())->GetRegNo(), FLOAT64);
        } else if (fcmp_ins->GetOp2()->GetOperandType() == BasicOperand::IMMF32) {
            cmp_op2 = fromImmF(((ImmF32Operand *)fcmp_ins->GetOp2())->GetFloatVal());
        }

        bool reverse = false;
        int rv_code;
        opcode = FcmpCond_2_op(fcmp_ins->GetCond(), reverse, rv_code);
        if (reverse) {
            // 交换操作数
            auto inst = rvconstructor->ConstructR(rv_code, set_result, cmp_op2, cmp_op1);
            cur_block->push_back(inst);
        } else {
            auto inst = rvconstructor->ConstructR(rv_code, set_result, cmp_op1, cmp_op2);
            cur_block->push_back(inst);
        }
        // 置位结果作为新的第一个操作数，0 作为新的第二个操作数
        cmp_op1 = set_result;
        cmp_op2 = GetPhysicalReg(RISCV_x0);
    }
    auto true_label = (LabelOperand *)ins->GetTrueLabel();
    auto false_label = (LabelOperand *)ins->GetFalseLabel();

    auto br_ins = rvconstructor->ConstructBLabel(opcode, cmp_op1, cmp_op2, RiscVLabel(true_label->GetLabelNo()));
    cur_block->push_back(br_ins);
    auto br_else_ins =
    rvconstructor->ConstructJLabel(RISCV_JAL, GetPhysicalReg(RISCV_x0), RiscVLabel(false_label->GetLabelNo()));
    cur_block->push_back(br_else_ins);
}

template <> void RiscV64Selector::ConvertAndAppend<BrUncondInstruction *>(BrUncondInstruction *ins) {
    // TODO("Implement this if you need");
    auto dest = ((LabelOperand *)ins->GetDestLabel())->GetLabelNo();
    auto jal_inst = rvconstructor->ConstructJLabel(RISCV_JAL, GetPhysicalReg(RISCV_x0), RiscVLabel(dest));
    cur_block->push_back(jal_inst);
}

// Reference
// https://gitlab.eduxiji.net/educg-group-26173-2487151/T202410055203113-826/-/blob/master/target/riscv64gc/instruction_select/riscv64_instSelect.cc:1130-1380
template <> void RiscV64Selector::ConvertAndAppend<CallInstruction *>(CallInstruction *ins) {
    // TODO("Implement this if you need");

    // 参数计数器
    int ireg_cnt = 0;
    int freg_cnt = 0;
    int stkpara_cnt = 0;

    // memset 函数处理
    if (ins->GetFunctionName() == std::string("llvm.memset.p0.i32")) {
        // parameter 0
        {
            auto ptrreg_op = (RegOperand *)ins->GetParameterList()[0].second;
            int ptrreg_no = ((RegOperand *)ins->GetParameterList()[0].second)->GetRegNo();
            if (alloca_off.find(ptrreg_op) == alloca_off.end()) {
                auto ptrreg = Register(true, ptrreg_no, INT64, false);
                cur_block->push_back(rvconstructor->ConstructIImm(RISCV_ADDI, GetPhysicalReg(RISCV_a0), ptrreg, 0));
            } else {
                auto sp_offset = alloca_off[ptrreg_op];
                auto reg = fromImm(sp_offset);
                auto ld_alloca =
                rvconstructor->ConstructR(RISCV_ADD, GetPhysicalReg(RISCV_a0), GetPhysicalReg(RISCV_sp), reg);
                cur_block->push_back(ld_alloca);
            }
        }
        // parameters 1
        {
            auto imm_op = (ImmI32Operand *)(ins->GetParameterList()[1].second);
            auto reg = fromImm(imm_op->GetIntImmVal());
            cur_block->push_back(
            rvconstructor->ConstructR(RISCV_ADD, GetPhysicalReg(RISCV_a1), GetPhysicalReg(RISCV_x0), reg));
        }

        // paramters 2
        {
            if (ins->GetParameterList()[2].second->GetOperandType() == BasicOperand::IMMI32) {
                int arr_sz = ((ImmI32Operand *)ins->GetParameterList()[2].second)->GetIntImmVal();
                auto reg = fromImm(arr_sz);
                cur_block->push_back(
                rvconstructor->ConstructR(RISCV_ADD, GetPhysicalReg(RISCV_a2), GetPhysicalReg(RISCV_x0), reg));
            } else {
                auto sizereg_op = (RegOperand *)ins->GetParameterList()[2].second;
                int sizereg_no = ((RegOperand *)ins->GetParameterList()[2].second)->GetRegNo();
                if (alloca_off.find(sizereg_op) == alloca_off.end()) {
                    auto sizereg = Register(true, sizereg_no, INT64, false);
                    cur_block->push_back(
                    rvconstructor->ConstructIImm(RISCV_ADDI, GetPhysicalReg(RISCV_a2), sizereg, 0));
                } else {
                    auto sp_offset = alloca_off[sizereg_op];
                    auto reg = fromImm(sp_offset);
                    auto ld_alloca =
                    rvconstructor->ConstructR(RISCV_ADD, GetPhysicalReg(RISCV_a2), GetPhysicalReg(RISCV_sp), reg);
                    cur_block->push_back(ld_alloca);
                }
            }
        }
        // call
        cur_block->push_back(rvconstructor->ConstructCall(RISCV_CALL, "memset", 3, 0));
        return;
    }

    // 遍历函数参数列表
    for (auto [type, arg_op] : ins->GetParameterList()) {
        // 处理 I32 类型和指针类型的参数
        if (type == BasicInstruction::I32 || type == BasicInstruction::PTR) {
            // 如果整数寄存器未用完
            if (ireg_cnt < 8) {
                if (arg_op->GetOperandType() == BasicOperand::REG) {
                    // 参数是寄存器类型
                    auto arg_regop = (RegOperand *)arg_op;
                    auto arg_reg = Register(true, arg_regop->GetRegNo(), INT64, false);
                    if (alloca_off.find(arg_regop) == alloca_off.end()) {
                        // 参数未被分配到栈空间
                        auto arg_copy_instr =
                        rvconstructor->ConstructIImm(RISCV_ADDI, GetPhysicalReg(RISCV_a0 + ireg_cnt), arg_reg, 0);
                        cur_block->push_back(arg_copy_instr);
                    } else {
                        // 参数已分配到栈空间，使用偏移量从栈中加载到寄存器
                        auto sp_offset = alloca_off[arg_regop];
                        auto sp_reg = fromImm(sp_offset);
                        cur_block->push_back(rvconstructor->ConstructR(RISCV_ADD, GetPhysicalReg(RISCV_a0 + ireg_cnt),
                                                                       GetPhysicalReg(RISCV_sp), sp_reg));
                    }
                } else if (arg_op->GetOperandType() == BasicOperand::IMMI32) {
                    // 参数是立即数类型
                    auto arg_immop = (ImmI32Operand *)arg_op;
                    auto arg_imm = arg_immop->GetIntImmVal();
                    auto arg_reg = fromImm(arg_imm);
                    auto arg_copy_inst = rvconstructor->ConstructR(RISCV_ADD, GetPhysicalReg(RISCV_a0 + ireg_cnt),
                                                                   GetPhysicalReg(RISCV_x0), arg_reg);
                    cur_block->push_back(arg_copy_inst);
                } else if (arg_op->GetOperandType() == BasicOperand::GLOBAL) {
                    // 参数是全局变量
                    auto mid_reg = cur_func->GetNewReg(INT64);
                    auto arg_global = (GlobalOperand *)arg_op;
                    cur_block->push_back(
                    rvconstructor->ConstructULabel(RISCV_LUI, mid_reg, RiscVLabel(arg_global->GetName(), true)));
                    cur_block->push_back(rvconstructor->ConstructILabel(RISCV_ADDI, GetPhysicalReg(RISCV_a0 + ireg_cnt),
                                                                        mid_reg,
                                                                        RiscVLabel(arg_global->GetName(), false)));
                }
            }
            ireg_cnt++;
        } else if (type == BasicInstruction::FLOAT32) {    // 处理 FLOAT32 类型参数
            if (freg_cnt < 8) {                            // 如果浮点寄存器未用完
                // 参数是寄存器类型
                if (arg_op->GetOperandType() == BasicOperand::REG) {
                    auto arg_regop = (RegOperand *)arg_op;
                    auto arg_reg = Register(true, arg_regop->GetRegNo(), FLOAT64, false);

                    auto zero = fromImmF(0);
                    // 将参数copy至参数寄存器
                    auto arg_copy_instr =
                    rvconstructor->ConstructR(RISCV_FADD_S, GetPhysicalReg(RISCV_fa0 + freg_cnt), arg_reg, zero);
                    cur_block->push_back(arg_copy_instr);
                } else if (arg_op->GetOperandType() == BasicOperand::IMMF32) {
                    // 参数是立即数类型
                    auto arg_imm = ((ImmF32Operand *)arg_op)->GetFloatVal();
                    auto arg = fromImmF(arg_imm);
                    auto zero = fromImmF(0);
                    // 将参数copy至参数寄存器
                    auto arg_copy_inst =
                    rvconstructor->ConstructR(RISCV_FADD_S, GetPhysicalReg(RISCV_fa0 + freg_cnt), arg, zero);
                    cur_block->push_back(arg_copy_inst);
                } else {
                    ERROR("Unexpected Operand type");
                }
            }
            freg_cnt++;
        } else {
            ERROR("Unexpected parameter type %d", type);
        }
    }

    // 计算需要分配在栈上的参数数量
    if (ireg_cnt - 8 > 0)
        stkpara_cnt += (ireg_cnt - 8);
    if (freg_cnt - 8 > 0)
        stkpara_cnt += (freg_cnt - 8);

    // 如果有参数需要分配到栈空间
    if (stkpara_cnt != 0) {
        ireg_cnt = freg_cnt = 0;
        int arg_off = 0;
        for (auto [type, arg_op] : ins->GetParameterList()) {
            if (type == BasicInstruction::I32 || type == BasicInstruction::PTR) {
                if (ireg_cnt < 8) {
                } else {
                    // 超出寄存器范围的整数参数分配到栈空间
                    if (arg_op->GetOperandType() == BasicOperand::REG) {
                        auto arg_regop = (RegOperand *)arg_op;
                        auto arg_reg = Register(true, arg_regop->GetRegNo(), INT64, false);
                        if (alloca_off.find(arg_regop) == alloca_off.end()) {
                            cur_block->push_back(
                            rvconstructor->ConstructSImm(RISCV_SD, arg_reg, GetPhysicalReg(RISCV_sp), arg_off));
                        } else {
                            auto sp_offset = alloca_off[arg_regop];
                            auto mid_reg = fromImm(sp_offset);
                            cur_block->push_back(
                            rvconstructor->ConstructSImm(RISCV_SD, mid_reg, GetPhysicalReg(RISCV_sp), arg_off));
                        }
                    } else if (arg_op->GetOperandType() == BasicOperand::IMMI32) {
                        auto arg_immop = (ImmI32Operand *)arg_op;
                        auto arg_imm = arg_immop->GetIntImmVal();
                        auto imm_reg = fromImm(arg_imm);
                        cur_block->push_back(
                        rvconstructor->ConstructSImm(RISCV_SD, imm_reg, GetPhysicalReg(RISCV_sp), arg_off));
                    } else if (arg_op->GetOperandType() == BasicOperand::GLOBAL) {
                        auto glb_reg1 = cur_func->GetNewReg(INT64);
                        auto glb_reg2 = cur_func->GetNewReg(INT64);
                        auto arg_glbop = (GlobalOperand *)arg_op;
                        cur_block->push_back(
                        rvconstructor->ConstructULabel(RISCV_LUI, glb_reg1, RiscVLabel(arg_glbop->GetName(), true)));
                        cur_block->push_back(rvconstructor->ConstructILabel(RISCV_ADDI, glb_reg2, glb_reg1,
                                                                            RiscVLabel(arg_glbop->GetName(), false)));
                        cur_block->push_back(
                        rvconstructor->ConstructSImm(RISCV_SD, glb_reg2, GetPhysicalReg(RISCV_sp), arg_off));
                    }
                    arg_off += 8;
                }
                ireg_cnt++;
            } else if (type == BasicInstruction::FLOAT32) {
                if (freg_cnt < 8) {
                } else {    // 超出寄存器范围的浮点参数分配到栈空间
                    if (arg_op->GetOperandType() == BasicOperand::REG) {
                        auto arg_regop = (RegOperand *)arg_op;
                        auto arg_reg = Register(true, arg_regop->GetRegNo(), FLOAT64, false);
                        cur_block->push_back(
                        rvconstructor->ConstructSImm(RISCV_FSD, arg_reg, GetPhysicalReg(RISCV_sp), arg_off));
                    } else if (arg_op->GetOperandType() == BasicOperand::IMMF32) {
                        auto arg_reg = fromImmF(((ImmF32Operand *)arg_op)->GetFloatVal());
                        cur_block->push_back(
                        rvconstructor->ConstructSImm(RISCV_FSD, arg_reg, GetPhysicalReg(RISCV_sp), arg_off));
                    }
                    arg_off += 8;
                }
                freg_cnt++;
            } else if (type == BasicInstruction::DOUBLE) {
                if (ireg_cnt < 8) {
                } else {
                    if (arg_op->GetOperandType() == BasicOperand::REG) {
                        auto arg_regop = (RegOperand *)arg_op;
                        auto arg_reg = Register(true, arg_regop->GetRegNo(), FLOAT64, false);
                        cur_block->push_back(
                        rvconstructor->ConstructSImm(RISCV_FSD, arg_reg, GetPhysicalReg(RISCV_sp), arg_off));
                    } else {
                        ERROR("Unexpected Operand type");
                    }
                    arg_off += 8;
                }
                ireg_cnt++;
            } else {
                ERROR("Unexpected parameter type %d", type);
            }
        }
    }

    // 插入调用指令
    // 保存 ra
    // int ra_offset = stkpara_cnt * 8;
    // auto save_ra_instr =
    // rvconstructor->ConstructSImm(RISCV_SD, GetPhysicalReg(RISCV_ra), GetPhysicalReg(RISCV_sp), ra_offset + 8);
    // cur_block->push_back(save_ra_instr);

    auto call_funcname = ins->GetFunctionName();
    if (ireg_cnt > 8) {
        ireg_cnt = 8;
    }
    if (freg_cnt > 8) {
        freg_cnt = 8;
    }
    // 处理返回值
    // Return Value
    auto return_type = ins->GetRetType();
    auto result_op = (RegOperand *)ins->GetResult();
    cur_block->push_back(rvconstructor->ConstructCall(RISCV_CALL, call_funcname, ireg_cnt, freg_cnt));
    cur_func->UpdateParaSize(stkpara_cnt * 8);
    if (return_type == BasicInstruction::I32) {
        auto result_reg = Register(true, result_op->GetRegNo(), INT64, false);
        auto copy_ret_ins = rvconstructor->ConstructIImm(RISCV_ADDIW, result_reg, GetPhysicalReg(RISCV_a0), 0);
        cur_block->push_back(copy_ret_ins);
    } else if (return_type == BasicInstruction::FLOAT32) {
        auto result_reg = Register(true, result_op->GetRegNo(), FLOAT64, false);
        auto zero = fromImmF(0);
        auto copy_ret_ins = rvconstructor->ConstructR(RISCV_FADD_S, result_reg, GetPhysicalReg(RISCV_fa0), zero);
        cur_block->push_back(copy_ret_ins);
    } else if (return_type == BasicInstruction::VOID) {
    } else {
        ERROR("Unexpected return type %d", return_type);
    }
    // 恢复 ra
    // auto restore_ra_instr =
    // rvconstructor->ConstructIImm(RISCV_LD, GetPhysicalReg(RISCV_ra), GetPhysicalReg(RISCV_sp), ra_offset + 8);
    // cur_block->push_back(restore_ra_instr);
}

template <> void RiscV64Selector::ConvertAndAppend<RetInstruction *>(RetInstruction *ins) {
    if (ins->GetRetVal() != NULL) {
        if (ins->GetRetVal()->GetOperandType() == BasicOperand::IMMI32) {
            auto retimm_op = (ImmI32Operand *)ins->GetRetVal();
            auto imm = retimm_op->GetIntImmVal();

            auto retcopy_instr = rvconstructor->ConstructUImm(RISCV_LI, GetPhysicalReg(RISCV_a0), imm);
            cur_block->push_back(retcopy_instr);
        } else if (ins->GetRetVal()->GetOperandType() == BasicOperand::IMMF32) {
            TODO("Implement this if you need");
        } else if (ins->GetRetVal()->GetOperandType() == BasicOperand::REG) {
            // TODO("Implement this if you need");

            auto ret_op = (RegOperand *)ins->GetRetVal();
            if (ins->GetType() == BasicInstruction::LLVMType::I32) {
                // addw a0, x0, reg
                Register reg(true, ret_op->GetRegNo(), INT64, false);
                auto add_ins =
                rvconstructor->ConstructR(RISCV_ADDW, GetPhysicalReg(RISCV_a0), GetPhysicalReg(RISCV_x0), reg);
                cur_block->push_back(add_ins);
                cur_func->ret_blocks.insert(cur_block);
            } else if (ins->GetType() == BasicInstruction::LLVMType::FLOAT32) {
                // TODO("其他返回类型处理");
                auto f0 = cur_func->GetNewReg(FLOAT64);
                auto fvct_inst = rvconstructor->ConstructR2(RISCV_FCVT_S_W, f0, GetPhysicalReg(RISCV_x0));
                cur_block->push_back(fvct_inst);
                Register reg(true, ret_op->GetRegNo(), FLOAT64, false);
                auto add_ins = rvconstructor->ConstructR(RISCV_FADD_S, GetPhysicalReg(RISCV_fa0), f0, reg);
                cur_block->push_back(add_ins);
            }
        }
    }

    auto ret_instr = rvconstructor->ConstructIImm(RISCV_JALR, GetPhysicalReg(RISCV_x0), GetPhysicalReg(RISCV_ra), 0);
    if (ins->GetType() == BasicInstruction::I32) {
        ret_instr->setRetType(1);
    } else if (ins->GetType() == BasicInstruction::FLOAT32) {
        ret_instr->setRetType(2);
    } else {
        ret_instr->setRetType(0);
    }
    cur_block->push_back(ret_instr);
}

template <> void RiscV64Selector::ConvertAndAppend<FptosiInstruction *>(FptosiInstruction *ins) {
    // TODO("Implement this if you need");

    // 浮点->整型
    if (ins->GetSrc()->GetOperandType() == BasicOperand::REG) {
        auto src_reg_no = ((RegOperand *)ins->GetSrc())->GetRegNo();
        auto dst_reg_no = ((RegOperand *)ins->GetResult())->GetRegNo();
        Register src(true, src_reg_no, FLOAT64, false);
        Register dst(true, dst_reg_no, INT64, false);
        auto fvct_inst = rvconstructor->ConstructR2(RISCV_FCVT_W_S, dst, src);
        cur_block->push_back(fvct_inst);
    } else if (ins->GetSrc()->GetOperandType() == BasicOperand::IMMF32) {
        Register src;
        auto dst_reg_no = ((RegOperand *)ins->GetResult())->GetRegNo();
        Register dst(true, dst_reg_no, INT64, false);
        auto imm = ((ImmF32Operand *)ins->GetSrc())->GetFloatVal();
        if (imm == 0) {
            src = GetPhysicalReg(RISCV_x0);
        } else {
            TODO("...");
            // src = cur_func->GetNewReg(FLOAT64);
            // auto imm2reg_inst = rvconstructor->ConstructIImm(RISCV_ADDIW, src, GetPhysicalReg(RISCV_x0), imm);
            // cur_block->push_back(imm2reg_inst);
        }
        auto fcvt_inst = rvconstructor->ConstructR2(RISCV_FCVT_W_S, dst, src);
        cur_block->push_back(fcvt_inst);
    }
}

template <> void RiscV64Selector::ConvertAndAppend<SitofpInstruction *>(SitofpInstruction *ins) {
    // TODO("Implement this if you need");

    // 整型->浮点
    if (ins->GetSrc()->GetOperandType() == BasicOperand::REG) {
        auto src_reg_no = ((RegOperand *)ins->GetSrc())->GetRegNo();
        auto dst_reg_no = ((RegOperand *)ins->GetResult())->GetRegNo();
        Register src(true, src_reg_no, INT64, false);
        Register dst(true, dst_reg_no, FLOAT64, false);
        auto fvct_inst = rvconstructor->ConstructR2(RISCV_FCVT_S_W, dst, src);
        cur_block->push_back(fvct_inst);
    } else if (ins->GetSrc()->GetOperandType() == BasicOperand::IMMI32) {
        Register src;
        auto dst_reg_no = ((RegOperand *)ins->GetResult())->GetRegNo();
        Register dst(true, dst_reg_no, FLOAT64, false);

        auto imm = ((ImmI32Operand *)ins->GetSrc())->GetIntImmVal();
        if (imm == 0) {
            src = GetPhysicalReg(RISCV_x0);
        } else {
            src = fromImm(imm);
        }
        auto fcvt_inst = rvconstructor->ConstructR2(RISCV_FCVT_S_W, dst, src);
        cur_block->push_back(fcvt_inst);
    }
}

template <> void RiscV64Selector::ConvertAndAppend<ZextInstruction *>(ZextInstruction *ins) {

    // I1 -> I32
    auto cmp_op = ins->GetSrc();
    auto cmp_ins = cmp_map[cmp_op];
    auto result_reg = Register(true, ((RegOperand *)ins->GetResult())->GetRegNo(), INT64, false);
    if (cmp_ins->GetOpcode() == BasicInstruction::LLVMIROpcode::ICMP) {
        auto icmp_ins = (IcmpInstruction *)cmp_ins;
        auto op1 = icmp_ins->GetOp1();
        auto op2 = icmp_ins->GetOp2();

        // 处理两个操作数: 转换到 float 进行比较
        Register cmp_op1, cmp_op2;
        Register cmp_fop1 = cur_func->GetNewReg(FLOAT64);
        Register cmp_fop2 = cur_func->GetNewReg(FLOAT64);
        if (icmp_ins->GetOp1()->GetOperandType() == BasicOperand::REG) {
            cmp_op1 = Register(true, ((RegOperand *)icmp_ins->GetOp1())->GetRegNo(), INT64, false);
            auto s2fp_inst = rvconstructor->ConstructR2(RISCV_FMV_W_X, cmp_fop1, cmp_op1);
            cur_block->push_back(s2fp_inst);
        } else if (icmp_ins->GetOp1()->GetOperandType() == BasicOperand::IMMI32) {
            int int_val = ((ImmI32Operand *)icmp_ins->GetOp1())->GetIntImmVal();
            if (int_val == 0) {
                auto s2fp_inst = rvconstructor->ConstructR2(RISCV_FMV_W_X, cmp_fop1, GetPhysicalReg(RISCV_x0));
                cur_block->push_back(s2fp_inst);
            } else {
                cmp_fop1 = fromImmF(int_val);
            }
        }
        if (icmp_ins->GetOp2()->GetOperandType() == BasicOperand::REG) {
            cmp_op2 = Register(true, ((RegOperand *)icmp_ins->GetOp2())->GetRegNo(), INT64, false);
            auto s2fp_inst = rvconstructor->ConstructR2(RISCV_FMV_W_X, cmp_fop2, cmp_op2);
            cur_block->push_back(s2fp_inst);
        } else if (icmp_ins->GetOp2()->GetOperandType() == BasicOperand::IMMI32) {
            int int_val = ((ImmI32Operand *)icmp_ins->GetOp2())->GetIntImmVal();
            if (int_val == 0) {
                auto s2fp_inst = rvconstructor->ConstructR2(RISCV_FMV_W_X, cmp_fop2, GetPhysicalReg(RISCV_x0));
                cur_block->push_back(s2fp_inst);
            } else {
                cmp_fop2 = fromImmF(int_val);
            }
        }

        // 从 IcmpCond 转换为 FcmpCond
        BasicInstruction::FcmpCond f_cond;
        switch (icmp_ins->GetCond()) {
        case BasicInstruction::eq:
            f_cond = BasicInstruction::FcmpCond::OEQ;
            break;
        case BasicInstruction::ne:
            f_cond = BasicInstruction::FcmpCond::ONE;
            break;
        case BasicInstruction::ugt:
            f_cond = BasicInstruction::FcmpCond::OGT;
            break;
        case BasicInstruction::uge:
            f_cond = BasicInstruction::FcmpCond::OGE;
            break;
        case BasicInstruction::ult:
            f_cond = BasicInstruction::FcmpCond::OLT;
            break;
        case BasicInstruction::ule:
            f_cond = BasicInstruction::FcmpCond::OLE;
            break;
        case BasicInstruction::sgt:
            f_cond = BasicInstruction::FcmpCond::OGT;
            break;
        case BasicInstruction::sge:
            f_cond = BasicInstruction::FcmpCond::OGE;
            break;
        case BasicInstruction::slt:
            f_cond = BasicInstruction::FcmpCond::OLT;
            break;
        case BasicInstruction::sle:
            f_cond = BasicInstruction::FcmpCond::OLE;
            break;
        }

        bool reverse = false;
        int rv_code;
        int op = FcmpCond_2_op(f_cond, reverse, rv_code);
        if (reverse) {
            // 交换操作数
            auto inst = rvconstructor->ConstructR(rv_code, result_reg, cmp_fop2, cmp_fop1);
            cur_block->push_back(inst);
        } else {
            auto inst = rvconstructor->ConstructR(rv_code, result_reg, cmp_fop1, cmp_fop2);
            cur_block->push_back(inst);
            if (f_cond == BrCondInstruction::FcmpCond::ONE) {
                cur_block->push_back(rvconstructor->ConstructIImm(RISCV_XORI, result_reg, result_reg, 1));
            }
        }

    } else if (cmp_ins->GetOpcode() == BasicInstruction::LLVMIROpcode::FCMP) {
        auto fcmp_ins = (FcmpInstruction *)cmp_ins;

        // 处理两个操作数
        Register cmp_op1, cmp_op2;
        if (fcmp_ins->GetOp1()->GetOperandType() == BasicOperand::REG) {
            cmp_op1 = Register(true, ((RegOperand *)fcmp_ins->GetOp1())->GetRegNo(), FLOAT64, false);
        } else if (fcmp_ins->GetOp1()->GetOperandType() == BasicOperand::IMMF32) {
            float float_val = ((ImmF32Operand *)fcmp_ins->GetOp1())->GetFloatVal();
            cmp_op1 = fromImmF(float_val);
        }
        if (fcmp_ins->GetOp2()->GetOperandType() == BasicOperand::REG) {
            cmp_op2 = Register(true, ((RegOperand *)fcmp_ins->GetOp2())->GetRegNo(), FLOAT64, false);
        } else if (fcmp_ins->GetOp2()->GetOperandType() == BasicOperand::IMMF32) {
            float float_val = ((ImmF32Operand *)fcmp_ins->GetOp2())->GetFloatVal();
            cmp_op2 = fromImmF(float_val);
        }

        bool reverse = false;
        int rv_code;
        int op = FcmpCond_2_op(fcmp_ins->GetCond(), reverse, rv_code);
        if (reverse) {
            // 交换操作数
            auto inst = rvconstructor->ConstructR(rv_code, result_reg, cmp_op2, cmp_op1);
            cur_block->push_back(inst);
        } else {
            auto inst = rvconstructor->ConstructR(rv_code, result_reg, cmp_op1, cmp_op2);
            cur_block->push_back(inst);
            if (fcmp_ins->GetCond() == BrCondInstruction::FcmpCond::ONE) {
                cur_block->push_back(rvconstructor->ConstructIImm(RISCV_XORI, result_reg, result_reg, 1));
            }
        }
    }
}

template <> void RiscV64Selector::ConvertAndAppend<GetElementptrInstruction *>(GetElementptrInstruction *ins) {
    // TODO("Implement this if you need");

    // 偏移量保存的寄存器
    auto offset_reg = cur_func->GetNewReg(INT64);
    auto ini_inst =
    rvconstructor->ConstructR(RISCV_ADD, offset_reg, GetPhysicalReg(RISCV_x0), GetPhysicalReg(RISCV_x0));
    cur_block->push_back(ini_inst);
    auto indexes = ins->GetIndexes();
    if (ins->GetIndexes().size() - 1 == ins->GetDims().size()) {
        if (indexes.size() == 1) {
            Register idx;
            int i = 0;
            if (indexes[i]->GetOperandType() == BasicOperand::IMMI32) {
                auto imm = ((ImmI32Operand *)indexes[i])->GetIntImmVal();
                idx = fromImm(imm);
            } else if (indexes[i]->GetOperandType() == BasicOperand::REG) {
                auto idx_reg_no = ((RegOperand *)indexes[i])->GetRegNo();
                idx = Register(true, idx_reg_no, INT64, false);
            }
            auto add_inst = rvconstructor->ConstructR(RISCV_ADD, offset_reg, offset_reg, idx);
            cur_block->push_back(add_inst);
        } else {
            indexes.erase(indexes.begin());
            for (int i = 0; i < indexes.size(); i++) {
                Register idx;
                if (indexes[i]->GetOperandType() == BasicOperand::IMMI32) {
                    auto imm = ((ImmI32Operand *)indexes[i])->GetIntImmVal();
                    idx = fromImm(imm);
                } else if (indexes[i]->GetOperandType() == BasicOperand::REG) {
                    auto idx_reg_no = ((RegOperand *)indexes[i])->GetRegNo();
                    idx = Register(true, idx_reg_no, INT64, false);
                }
                // 确保对 idx 是只读的
                Register tmp = cur_func->GetNewReg(INT64);
                auto copy_idx_inst = rvconstructor->ConstructR(RISCV_ADD, tmp, GetPhysicalReg(RISCV_x0), idx);
                cur_block->push_back(copy_idx_inst);
                if (i != indexes.size() - 1) {
                    /*
                        对于访问多维数组 int arr[a][b][c]
                        x = arr[1][2][3];
                        则计算偏移量为 1 * (b * c) + (2 * b) + 3
                    */
                    int last_dim_product = 1;
                    for (int j = i + 1; j < indexes.size(); j++) {
                        last_dim_product *= ins->GetDims()[j];
                    }
                    auto imm_reg = fromImm(last_dim_product);
                    auto mul_inst = rvconstructor->ConstructR(RISCV_MULW, tmp, tmp, imm_reg);
                    cur_block->push_back(mul_inst);
                }
                auto add_inst = rvconstructor->ConstructR(RISCV_ADD, offset_reg, offset_reg, tmp);
                cur_block->push_back(add_inst);
            }
        }

        // 偏移量 * 4
        auto slli_inst = rvconstructor->ConstructIImm(RISCV_SLLI, offset_reg, offset_reg, 2);
        cur_block->push_back(slli_inst);
    } else if (ins->GetIndexes().size() == ins->GetDims().size()) {
        // TODO("...");
        for (int i = 0; i < indexes.size(); i++) {
            Register idx;
            if (indexes[i]->GetOperandType() == BasicOperand::IMMI32) {
                auto imm = ((ImmI32Operand *)indexes[i])->GetIntImmVal();
                idx = fromImm(imm);
            } else if (indexes[i]->GetOperandType() == BasicOperand::REG) {
                auto idx_reg_no = ((RegOperand *)indexes[i])->GetRegNo();
                idx = Register(true, idx_reg_no, INT64, false);
            }

            // 确保对 idx 是只读的
            Register tmp = cur_func->GetNewReg(INT64);
            auto copy_idx_inst = rvconstructor->ConstructR(RISCV_ADD, tmp, GetPhysicalReg(RISCV_x0), idx);
            cur_block->push_back(copy_idx_inst);
            if (i != indexes.size() - 1) {
                /*
                    对于访问多维数组 int arr[a][b][c]
                    x = arr[1][2][3];
                    则计算偏移量为 1 * (b * c) + (2 * b) + 3
                */
                int last_dim_product = 1;
                for (int j = i + 1; j < indexes.size(); j++) {
                    last_dim_product *= ins->GetDims()[j];
                }
                auto imm_reg = fromImm(last_dim_product);
                auto mul_inst = rvconstructor->ConstructR(RISCV_MULW, tmp, tmp, imm_reg);
                cur_block->push_back(mul_inst);
            }
            auto add_inst = rvconstructor->ConstructR(RISCV_ADD, offset_reg, offset_reg, tmp);
            cur_block->push_back(add_inst);
        }
        // 偏移量 * 4
        auto slli_inst = rvconstructor->ConstructIImm(RISCV_SLLI, offset_reg, offset_reg, 2);
        cur_block->push_back(slli_inst);
    }
    if (ins->GetPtrVal()->GetOperandType() == BasicOperand::GLOBAL) {
        auto ptr = (GlobalOperand *)ins->GetPtrVal();
        // 获取数组首地址
        // lui	    tmp,%hi(c)
        // addi		base,tmp,%lo(c)
        auto base = cur_func->GetNewReg(INT64);
        auto tmp = cur_func->GetNewReg(INT64);
        auto lui_inst = rvconstructor->ConstructULabel(RISCV_LUI, tmp, RiscVLabel(ptr->GetName(), true));
        cur_block->push_back(lui_inst);

        auto addi_inst = rvconstructor->ConstructILabel(RISCV_ADDI, base, tmp, RiscVLabel(ptr->GetName(), false));
        cur_block->push_back(addi_inst);

        auto rd_reg_no = ((RegOperand *)ins->GetResult())->GetRegNo();
        Register rd(true, rd_reg_no, INT64, false);
        auto inst = rvconstructor->ConstructR(RISCV_ADDW, rd, base, offset_reg);
        cur_block->push_back(inst);

    } else if (ins->GetPtrVal()->GetOperandType() == BasicOperand::REG) {
        /*
        // TODO("...");

        // 偏移量保存的寄存器
        auto offset_reg = cur_func->GetNewReg(INT64);
        auto ini_inst =
        rvconstructor->ConstructR(RISCV_ADDW, offset_reg, GetPhysicalReg(RISCV_x0), GetPhysicalReg(RISCV_x0));
        cur_block->push_back(ini_inst);
        auto indexes = ins->GetIndexes();
        if (ins->GetIndexes().size() - 1 == ins->GetDims().size()) {
            if (indexes.size() == 1) {
                Register idx;
                int i = 0;
                if (indexes[i]->GetOperandType() == BasicOperand::IMMI32) {
                    // std::cout << "here\n";
                    auto imm = ((ImmI32Operand *)indexes[i])->GetIntImmVal();
                    idx = fromImm(imm);
                    // idx = cur_func->GetNewReg(INT64);
                    // auto addi_inst = rvconstructor->ConstructIImm(RISCV_ADDIW, idx, GetPhysicalReg(RISCV_x0), imm);
                    // cur_block->push_back(addi_inst);
                } else if (indexes[i]->GetOperandType() == BasicOperand::REG) {
                    auto idx_reg_no = ((RegOperand *)indexes[i])->GetRegNo();
                    idx = Register(true, idx_reg_no, INT64, false);
                }
                auto add_inst = rvconstructor->ConstructR(RISCV_ADD, offset_reg, offset_reg, idx);
                cur_block->push_back(add_inst);
            } else {
                indexes.erase(indexes.begin());
                for (int i = 0; i < indexes.size(); i++) {
                    Register idx;

                    if (indexes[i]->GetOperandType() == BasicOperand::IMMI32) {
                        auto imm = ((ImmI32Operand *)indexes[i])->GetIntImmVal();
                        idx = fromImm(imm);
                        // idx = cur_func->GetNewReg(INT64);
                        // auto addi_inst = rvconstructor->ConstructIImm(RISCV_ADDIW, idx, GetPhysicalReg(RISCV_x0),
                        // imm); cur_block->push_back(addi_inst); tmp = idx;
                    } else if (indexes[i]->GetOperandType() == BasicOperand::REG) {
                        auto idx_reg_no = ((RegOperand *)indexes[i])->GetRegNo();
                        idx = Register(true, idx_reg_no, INT64, false);
                        // tmp = cur_func->GetNewReg(INT64);
                    }
                    // 确保对 idx 是只读的
                    Register tmp = cur_func->GetNewReg(INT64);
                    auto copy_idx_inst = rvconstructor->ConstructR(RISCV_ADD, tmp, GetPhysicalReg(RISCV_x0), idx);
                    cur_block->push_back(copy_idx_inst);
                    if (i != indexes.size() - 1) {
                        auto imm_reg = fromImm(ins->GetDims()[i]);
                        auto mul_inst = rvconstructor->ConstructR(RISCV_MULW, tmp, tmp, imm_reg);
                        cur_block->push_back(mul_inst);
                    }
                    auto add_inst = rvconstructor->ConstructR(RISCV_ADD, offset_reg, offset_reg, tmp);
                    cur_block->push_back(add_inst);
                }
            }

            // 偏移量 * 4
            auto slli_inst = rvconstructor->ConstructIImm(RISCV_SLLI, offset_reg, offset_reg, 2);
            cur_block->push_back(slli_inst);
        } else if (ins->GetIndexes().size() == ins->GetDims().size()) {
            // TODO("...");
            for (int i = 0; i < indexes.size(); i++) {
                Register idx;
                // Register tmp;
                if (indexes[i]->GetOperandType() == BasicOperand::IMMI32) {
                    auto imm = ((ImmI32Operand *)indexes[i])->GetIntImmVal();
                    idx = fromImm(imm);
                    // idx = cur_func->GetNewReg(INT64);
                    // auto addi_inst = rvconstructor->ConstructIImm(RISCV_ADDIW, idx, GetPhysicalReg(RISCV_x0), imm);
                    // cur_block->push_back(addi_inst);
                    // tmp = idx;
                } else if (indexes[i]->GetOperandType() == BasicOperand::REG) {
                    auto idx_reg_no = ((RegOperand *)indexes[i])->GetRegNo();
                    idx = Register(true, idx_reg_no, INT64, false);
                    // tmp = cur_func->GetNewReg(INT64);
                }

                // 确保对 idx 是只读的
                Register tmp = cur_func->GetNewReg(INT64);
                auto copy_idx_inst = rvconstructor->ConstructR(RISCV_ADD, tmp, GetPhysicalReg(RISCV_x0), idx);
                cur_block->push_back(copy_idx_inst);
                if (i != indexes.size() - 1) {
                    auto imm_reg = fromImm(ins->GetDims()[i]);
                    auto mul_inst = rvconstructor->ConstructR(RISCV_MULW, tmp, tmp, imm_reg);
                    cur_block->push_back(mul_inst);
                }
                auto add_inst = rvconstructor->ConstructR(RISCV_ADD, offset_reg, offset_reg, tmp);
                cur_block->push_back(add_inst);
            }
            // 偏移量 * 4
            auto slli_inst = rvconstructor->ConstructIImm(RISCV_SLLI, offset_reg, offset_reg, 2);
            cur_block->push_back(slli_inst);
        } */
        auto ptr = (RegOperand *)ins->GetPtrVal();

        auto base = cur_func->GetNewReg(INT64);

        if (ins->GetType() == BasicInstruction::I32) {
            if (alloca_off.find(ptr) != alloca_off.end()) {
                auto offset = alloca_off[ptr];
                auto reg = fromImm(offset);
                // base = sp + offset;
                auto add_inst = rvconstructor->ConstructR(RISCV_ADD, base, GetPhysicalReg(RISCV_sp), reg);
                cur_block->push_back(add_inst);
            } else {
                // TODO("未记录alloca的寄存器");
                Register tmp(true, ptr->GetRegNo(), INT64, false);
                auto lw_inst = rvconstructor->ConstructR(RISCV_ADD, base, tmp, GetPhysicalReg(RISCV_x0));
                cur_block->push_back(lw_inst);
            }
        } else if (ins->GetType() == BasicInstruction::FLOAT32) {
            // TODO("其他类型实现");
            if (alloca_off.find(ptr) != alloca_off.end()) {
                auto offset = alloca_off[ptr];
                auto reg = fromImm(offset);
                auto lw_inst = rvconstructor->ConstructR(RISCV_ADD, base, GetPhysicalReg(RISCV_sp), reg);
                cur_block->push_back(lw_inst);
            } else {
                // TODO("未记录alloca的寄存器");
                Register tmp(true, ptr->GetRegNo(), INT64, false);
                auto lw_inst = rvconstructor->ConstructR(RISCV_ADD, base, tmp, GetPhysicalReg(0));
                cur_block->push_back(lw_inst);
            }
        }

        auto rd_reg_no = ((RegOperand *)ins->GetResult())->GetRegNo();
        Register rd(true, rd_reg_no, INT64, false);
        auto inst = rvconstructor->ConstructR(RISCV_ADD, rd, base, offset_reg);
        cur_block->push_back(inst);
    } else {
        TODO("here\n");
    }
}

template <> void RiscV64Selector::ConvertAndAppend<PhiInstruction *>(PhiInstruction *ins) {
    TODO("Implement this if you need");
}

template <> void RiscV64Selector::ConvertAndAppend<Instruction>(Instruction inst) {
    switch (inst->GetOpcode()) {
    case BasicInstruction::LOAD:
        ConvertAndAppend<LoadInstruction *>((LoadInstruction *)inst);
        break;
    case BasicInstruction::STORE:
        ConvertAndAppend<StoreInstruction *>((StoreInstruction *)inst);
        break;
    case BasicInstruction::ADD:
    case BasicInstruction::SUB:
    case BasicInstruction::MUL:
    case BasicInstruction::DIV:
    case BasicInstruction::FADD:
    case BasicInstruction::FSUB:
    case BasicInstruction::FMUL:
    case BasicInstruction::FDIV:
    case BasicInstruction::MOD:
    case BasicInstruction::SHL:
    case BasicInstruction::BITXOR:
        ConvertAndAppend<ArithmeticInstruction *>((ArithmeticInstruction *)inst);
        break;
    case BasicInstruction::ICMP:
        ConvertAndAppend<IcmpInstruction *>((IcmpInstruction *)inst);
        break;
    case BasicInstruction::FCMP:
        ConvertAndAppend<FcmpInstruction *>((FcmpInstruction *)inst);
        break;
    case BasicInstruction::ALLOCA:
        ConvertAndAppend<AllocaInstruction *>((AllocaInstruction *)inst);
        break;
    case BasicInstruction::BR_COND:
        ConvertAndAppend<BrCondInstruction *>((BrCondInstruction *)inst);
        break;
    case BasicInstruction::BR_UNCOND:
        ConvertAndAppend<BrUncondInstruction *>((BrUncondInstruction *)inst);
        break;
    case BasicInstruction::RET:
        ConvertAndAppend<RetInstruction *>((RetInstruction *)inst);
        break;
    case BasicInstruction::ZEXT:
        ConvertAndAppend<ZextInstruction *>((ZextInstruction *)inst);
        break;
    case BasicInstruction::FPTOSI:
        ConvertAndAppend<FptosiInstruction *>((FptosiInstruction *)inst);
        break;
    case BasicInstruction::SITOFP:
        ConvertAndAppend<SitofpInstruction *>((SitofpInstruction *)inst);
        break;
    case BasicInstruction::GETELEMENTPTR:
        ConvertAndAppend<GetElementptrInstruction *>((GetElementptrInstruction *)inst);
        break;
    case BasicInstruction::CALL:
        ConvertAndAppend<CallInstruction *>((CallInstruction *)inst);
        break;
    case BasicInstruction::PHI:
        ConvertAndAppend<PhiInstruction *>((PhiInstruction *)inst);
        break;
    default:
        ERROR("Unknown LLVM IR instruction");
    }
}

void RiscV64Selector::SelectInstructionAndBuildCFG() {
    // 与中间代码生成一样, 如果你完全无从下手, 可以先看看输出是怎么写的
    // 即riscv64gc/instruction_print/*  common/machine_passes/machine_printer.h

    // 指令选择除了一些函数调用约定必须遵守的情况需要物理寄存器，其余情况必须均为虚拟寄存器
    dest->global_def = IR->global_def;
    // 遍历每个LLVM IR函数
    for (auto [defI, cfg] : IR->llvm_cfg) {
        if (cfg == nullptr) {
            ERROR("LLVMIR CFG is Empty, you should implement BuildCFG in MidEnd first");
        }
        std::string name = cfg->function_def->GetFunctionName();

        cur_func = new RiscV64Function(name);
        cur_func->SetParent(dest);
        // 你可以使用cur_func->GetNewRegister来获取新的虚拟寄存器
        dest->functions.push_back(cur_func);

        auto cur_mcfg = new MachineCFG;
        cur_func->SetMachineCFG(cur_mcfg);

        // 清空指令选择状态(可能需要自行添加初始化操作)
        ClearFunctionSelectState();

        // TODO: 添加函数参数(推荐先阅读一下riscv64_lowerframe.cc中的代码和注释)
        // See MachineFunction::AddParameter()
        // TODO("Add function parameter if you need");

        for (int i = 0; i < defI->formals.size(); i++) {
            // std::cout << "here\n";
            if (defI->formals[i] == BasicInstruction::I32 || defI->formals[i] == BasicInstruction::PTR) {
                cur_func->AddParameter(Register(true, ((RegOperand *)defI->formals_reg[i])->GetRegNo(), INT64, false));
            } else if (defI->formals[i] == BasicInstruction::FLOAT32) {
                cur_func->AddParameter(
                Register(true, ((RegOperand *)defI->formals_reg[i])->GetRegNo(), FLOAT64, false));
            } else {
                TODO("异常");
            }
        }

        // 遍历每个LLVM IR基本块
        for (auto [id, block] : *(cfg->block_map)) {
            cur_block = new RiscV64Block(id);
            // 将新块添加到Machine CFG中
            cur_mcfg->AssignEmptyNode(id, cur_block);
            cur_func->UpdateMaxLabel(id);

            cur_block->setParent(cur_func);
            cur_func->blocks.push_back(cur_block);

            // 指令选择主要函数, 请注意指令选择时需要维护变量cur_offset
            for (auto instruction : block->Instruction_list) {
                // Log("Selecting Instruction");
                ConvertAndAppend<Instruction>(instruction);
            }
        }

        // RISCV 8字节对齐（）
        if (cur_offset % 8 != 0) {
            cur_offset = ((cur_offset + 7) / 8) * 8;
        }
        cur_func->SetStackSize(cur_offset + cur_func->GetParaSize());

        // 控制流图连边
        for (int i = 0; i < cfg->G.size(); i++) {
            const auto &arcs = cfg->G[i];
            for (auto arc : arcs) {
                cur_mcfg->MakeEdge(i, arc->block_id);
            }
        }
    }
}

void RiscV64Selector::ClearFunctionSelectState() {
    cur_offset = 0;
    alloca_off.clear();
    cmp_map.clear();
}
