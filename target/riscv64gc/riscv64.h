#ifndef RISCV64_H
#define RISCV64_H
#include "../common/MachineBaseInstruction.h"
#include "../common/machine_passes/register_alloc/basic_register_allocation.h"
#include "../common/machine_passes/register_alloc/physical_register.h"

#pragma GCC diagnostic ignored "-Wwritable-strings"
#pragma GCC diagnostic ignored "-Wc99-designator"

// 这里只提供了少量伪指令，如果你需要更多的伪值令，你需要自行添加并编写对应的输出
// 1. 在下面的枚举类型中添加指令名
// 2. 在riscv64.cc文件中正确填写OpTable
enum {
    RISCV_SLL,
    RISCV_SLLI,
    RISCV_SRL,
    RISCV_SRLI,
    RISCV_SRA,
    RISCV_SRAI,
    RISCV_ADD,
    RISCV_ADDI,
    RISCV_SUB,
    RISCV_LUI,
    RISCV_AUIPC,
    RISCV_XOR,
    RISCV_XORI,
    RISCV_OR,
    RISCV_ORI,
    RISCV_AND,
    RISCV_ANDI,
    RISCV_SLT,
    RISCV_SLTI,
    RISCV_SLTU,
    RISCV_SLTIU,
    RISCV_BEQ,
    RISCV_BNE,
    RISCV_BLT,
    RISCV_BGE,
    RISCV_BLTU,
    RISCV_BGEU,
    RISCV_JAL,
    RISCV_JALR,
    RISCV_LB,
    RISCV_LH,
    RISCV_LBU,
    RISCV_LHU,
    RISCV_LW,
    RISCV_SB,
    RISCV_SH,
    RISCV_SW,
    RISCV_SLLW,
    RISCV_SLLIW,
    RISCV_SRLW,
    RISCV_SRLIW,
    RISCV_SRAW,
    RISCV_SRAIW,
    RISCV_ADDW,
    RISCV_ADDIW,
    RISCV_SUBW,
    RISCV_LWU,
    RISCV_LD,
    RISCV_SD,

    RISCV_MUL,
    RISCV_MULH,
    RISCV_MULHSU,
    RISCV_MULHU,
    RISCV_DIV,
    RISCV_DIVU,
    RISCV_REM,
    RISCV_REMU,
    RISCV_MULW,
    RISCV_DIVW,
    RISCV_REMW,
    RISCV_REMUW,

    RISCV_FMV_W_X,
    RISCV_FMV_X_W,
    RISCV_FCVT_S_W,
    RISCV_FCVT_D_W,
    RISCV_FCVT_S_WU,
    RISCV_FCVT_D_WU,
    RISCV_FCVT_W_S,
    RISCV_FCVT_W_D,
    RISCV_FCVT_WU_S,
    RISCV_FCVT_WU_D,
    RISCV_FLW,
    RISCV_FLD,
    RISCV_FSW,
    RISCV_FSD,
    RISCV_FADD_S,
    RISCV_FADD_D,
    RISCV_FSUB_S,
    RISCV_FSUB_D,
    RISCV_FMUL_S,
    RISCV_FMUL_D,
    RISCV_FDIV_S,
    RISCV_FDIV_D,
    RISCV_FSQRT_S,
    RISCV_FSQRT_D,
    RISCV_FMADD_S,
    RISCV_FMADD_D,
    RISCV_FMSUB_S,
    RISCV_FMSUB_D,
    RISCV_FNMSUB_S,
    RISCV_FNMSUB_D,
    RISCV_FNMADD_S,
    RISCV_FNMADD_D,
    RISCV_FSGNJ_S,
    RISCV_FSGNJ_D,
    RISCV_FSGNJN_S,
    RISCV_FSGNJN_D,
    RISCV_FSGNJX_S,
    RISCV_FSGNJX_D,
    RISCV_FMIN_S,
    RISCV_FMIN_D,
    RISCV_FMAX_S,
    RISCV_FMAX_D,
    RISCV_FEQ_S,
    RISCV_FEQ_D,
    RISCV_FLT_S,
    RISCV_FLT_D,
    RISCV_FLE_S,
    RISCV_FLE_D,
    RISCV_FCLASS_S,
    RISCV_FCLASS_D,
    RISCV_FMV_D_X,
    RISCV_FMV_X_D,
    RISCV_FCVT_S_L,
    RISCV_FCVT_D_L,
    RISCV_FCVT_S_LU,
    RISCV_FCVT_D_LU,
    RISCV_FCVT_L_S,
    RISCV_FCVT_L_D,
    RISCV_FCVT_LU_S,
    RISCV_FCVT_LU_D,

    RISCV_LI,
    RISCV_CALL,

    RISCV_BGT,
    RISCV_BLE,
    RISCV_BGTU,
    RISCV_BLEU,

    RISCV_FMV_S,
    RISCV_FMV_D,

    RISCV_SH1ADD,
    RISCV_SH2ADD,
    RISCV_SH3ADD,

    RISCV_SH1ADDUW,
    RISCV_SH2ADDUW,
    RISCV_SH3ADDUW,

    RISCV_MIN,
    RISCV_MAX,
    RISCV_MINU,
    RISCV_MAXU,

    RISCV_FCVT_D_S,
    RISCV_ZEXT_W,

    RISCV_FNEG_S,
    RISCV_FNEG_D,
};

struct RvOpInfo {
    enum {
        R_type,
        I_type,
        S_type,
        B_type,
        U_type,
        J_type,
        R2_type,
        R4_type,
        CALL_type,
    };
    int ins_formattype;    // 指令类型
    char *name;
    int latency;    // sifive-u74上的硬件指令延迟, 可以用于指令调度优化, 如果你不打算实现该优化可以忽略
};
extern RvOpInfo OpTable[];
enum {
    RISCV_x0,
    RISCV_x1,
    RISCV_x2,
    RISCV_x3,
    RISCV_x4,
    RISCV_x5,
    RISCV_x6,
    RISCV_x7,
    RISCV_x8,
    RISCV_x9,
    RISCV_x10,
    RISCV_x11,
    RISCV_x12,
    RISCV_x13,
    RISCV_x14,
    RISCV_x15,
    RISCV_x16,
    RISCV_x17,
    RISCV_x18,
    RISCV_x19,
    RISCV_x20,
    RISCV_x21,
    RISCV_x22,
    RISCV_x23,
    RISCV_x24,
    RISCV_x25,
    RISCV_x26,
    RISCV_x27,
    RISCV_x28,
    RISCV_x29,
    RISCV_x30,
    RISCV_x31,
    RISCV_f0,
    RISCV_f1,
    RISCV_f2,
    RISCV_f3,
    RISCV_f4,
    RISCV_f5,
    RISCV_f6,
    RISCV_f7,
    RISCV_f8,
    RISCV_f9,
    RISCV_f10,
    RISCV_f11,
    RISCV_f12,
    RISCV_f13,
    RISCV_f14,
    RISCV_f15,
    RISCV_f16,
    RISCV_f17,
    RISCV_f18,
    RISCV_f19,
    RISCV_f20,
    RISCV_f21,
    RISCV_f22,
    RISCV_f23,
    RISCV_f24,
    RISCV_f25,
    RISCV_f26,
    RISCV_f27,
    RISCV_f28,
    RISCV_f29,
    RISCV_f30,
    RISCV_f31,
    RISCV_INVALID,
    RISCV_spilled_in_memory,
};
enum {
    RISCV_ra = RISCV_x1,
    RISCV_sp = RISCV_x2,
    RISCV_gp = RISCV_x3,
    RISCV_tp = RISCV_x4,
    RISCV_t0 = RISCV_x5,
    RISCV_t1 = RISCV_x6,
    RISCV_t2 = RISCV_x7,
    RISCV_s0 = RISCV_x8,
    RISCV_s1 = RISCV_x9,
    RISCV_a0 = RISCV_x10,
    RISCV_a1 = RISCV_x11,
    RISCV_a2 = RISCV_x12,
    RISCV_a3 = RISCV_x13,
    RISCV_a4 = RISCV_x14,
    RISCV_a5 = RISCV_x15,
    RISCV_a6 = RISCV_x16,
    RISCV_a7 = RISCV_x17,
    RISCV_s2 = RISCV_x18,
    RISCV_s3 = RISCV_x19,
    RISCV_s4 = RISCV_x20,
    RISCV_s5 = RISCV_x21,
    RISCV_s6 = RISCV_x22,
    RISCV_s7 = RISCV_x23,
    RISCV_s8 = RISCV_x24,
    RISCV_s9 = RISCV_x25,
    RISCV_s10 = RISCV_x26,
    RISCV_s11 = RISCV_x27,
    RISCV_t3 = RISCV_x28,
    RISCV_t4 = RISCV_x29,
    RISCV_t5 = RISCV_x30,
    RISCV_t6 = RISCV_x31,
};

enum {
    RISCV_fp = RISCV_x8,
};

enum {
    RISCV_ft0 = RISCV_f0,
    RISCV_ft1 = RISCV_f1,
    RISCV_ft2 = RISCV_f2,
    RISCV_ft3 = RISCV_f3,
    RISCV_ft4 = RISCV_f4,
    RISCV_ft5 = RISCV_f5,
    RISCV_ft6 = RISCV_f6,
    RISCV_ft7 = RISCV_f7,
    RISCV_fs0 = RISCV_f8,
    RISCV_fs1 = RISCV_f9,
    RISCV_fa0 = RISCV_f10,
    RISCV_fa1 = RISCV_f11,
    RISCV_fa2 = RISCV_f12,
    RISCV_fa3 = RISCV_f13,
    RISCV_fa4 = RISCV_f14,
    RISCV_fa5 = RISCV_f15,
    RISCV_fa6 = RISCV_f16,
    RISCV_fa7 = RISCV_f17,
    RISCV_fs2 = RISCV_f18,
    RISCV_fs3 = RISCV_f19,
    RISCV_fs4 = RISCV_f20,
    RISCV_fs5 = RISCV_f21,
    RISCV_fs6 = RISCV_f22,
    RISCV_fs7 = RISCV_f23,
    RISCV_fs8 = RISCV_f24,
    RISCV_fs9 = RISCV_f25,
    RISCV_fs10 = RISCV_f26,
    RISCV_fs11 = RISCV_f27,
    RISCV_ft8 = RISCV_f28,
    RISCV_ft9 = RISCV_f29,
    RISCV_ft10 = RISCV_f30,
    RISCV_ft11 = RISCV_f31,
};

static inline MachineDataType getRVRegType(int reg_no) {
    if (reg_no >= RISCV_x0 && reg_no <= RISCV_x31) {
        return INT64;
    }
    if (reg_no >= RISCV_f0 && reg_no <= RISCV_f31) {
        return FLOAT64;
    }
    ERROR("Unknown reg_no %d", reg_no);
}

static inline Register GetPhysicalReg(int reg_no) { return Register(false, reg_no, getRVRegType(reg_no)); }

struct RiscV64RegisterInfo {
    char *name;
};
extern RiscV64RegisterInfo RiscV64Registers[];
extern Register RISCVregs[];

/*
一般情况下，可以认为中端条件跳转语句的前一条指令一定是icmp或fcmp
如果你的中端实现不是这样，需要自行探索一下条件跳转语句的翻译方式

%r3 = icmp ne i32 %r1, %r2
br i1 %r3, label %L1, label %L2
应当被翻译为:
bne %1, %2, .L1(即jmp_label_id)
j L2
*/
struct RiscVLabel {
    int jmp_label_id = 0;            // 该id为跳转的基本块编号
    bool is_data_address = false;    // 是否为数据段标签
    std::string name;
    bool is_hi;    // 对应%hi(name) 和 %lo(name)
    RiscVLabel() = default;
    RiscVLabel(std::string name, bool is_hi) : name(name), is_hi(is_hi) { this->is_data_address = true; }
    // 添加一些你想用的构造函数
    RiscVLabel(int dest) : name(), jmp_label_id(dest) { this->is_data_address = false; }
};

class RiscV64Instruction : public MachineBaseInstruction {
private:
    int op;
    Register rd, rs1, rs2, rs3;
    bool use_label;
    int imm;
    RiscVLabel label;

    // 下面两个变量的具体作用见ConstructCall函数
    int callireg_num;
    int callfreg_num;

    int ret_type;    // 用于GetI_typeReadreg(), 即确定函数返回时用的是a0寄存器还是fa0寄存器, 或者没有返回值

    std::vector<Register *> GetR_typeReadreg() { return {&rs1, &rs2}; }
    std::vector<Register *> GetR2_typeReadreg() { return {&rs1}; }
    std::vector<Register *> GetR4_typeReadreg() { return {&rs1, &rs2, &rs3}; }
    std::vector<Register *> GetI_typeReadreg() {
        std::vector<Register *> ret;
        ret.push_back(&rs1);
        if (op == RISCV_JALR) {
            // 当ret_type为1或2时, 我们认为jalr只会用于函数返回, 所以jalr会读取a0或fa0寄存器(即函数返回值)
            // 如果函数没有返回值或者你在其他地方使用到了jalr指令，将ret_type设置为0即可
            if (ret_type == 1) {
                ret.push_back(&RISCVregs[RISCV_a0]);
            } else if (ret_type == 2) {
                ret.push_back(&RISCVregs[RISCV_fa0]);
            }
        }
        return ret;
    }
    std::vector<Register *> GetS_typeReadreg() { return {&rs1, &rs2}; }
    std::vector<Register *> GetB_typeReadreg() { return {&rs1, &rs2}; }
    std::vector<Register *> GetU_typeReadreg() { return {}; }
    std::vector<Register *> GetJ_typeReadreg() { return {}; }
    std::vector<Register *> GetCall_typeReadreg() {
        std::vector<Register *> ret;
        for (int i = 0; i < callireg_num; i++) {
            ret.push_back(&RISCVregs[RISCV_a0 + i]);
        }
        for (int i = 0; i < callfreg_num; i++) {
            ret.push_back(&RISCVregs[RISCV_fa0 + i]);
        }
        return ret;
    }

    std::vector<Register *> GetR_typeWritereg() { return {&rd}; }
    std::vector<Register *> GetR2_typeWritereg() { return {&rd}; }
    std::vector<Register *> GetR4_typeWritereg() { return {&rd}; }
    std::vector<Register *> GetI_typeWritereg() { return {&rd}; }
    std::vector<Register *> GetS_typeWritereg() { return {}; }
    std::vector<Register *> GetB_typeWritereg() { return {}; }
    std::vector<Register *> GetU_typeWritereg() { return {&rd}; }
    std::vector<Register *> GetJ_typeWritereg() { return {&rd}; }
    std::vector<Register *> GetCall_typeWritereg() {
        return {
        &RISCVregs[RISCV_ra],

        &RISCVregs[RISCV_t0],  &RISCVregs[RISCV_t1],  &RISCVregs[RISCV_t2],   &RISCVregs[RISCV_t3],
        &RISCVregs[RISCV_t4],  &RISCVregs[RISCV_t5],  &RISCVregs[RISCV_t6],

        &RISCVregs[RISCV_a0],  &RISCVregs[RISCV_a1],  &RISCVregs[RISCV_a2],   &RISCVregs[RISCV_a3],
        &RISCVregs[RISCV_a4],  &RISCVregs[RISCV_a5],  &RISCVregs[RISCV_a6],   &RISCVregs[RISCV_a7],

        &RISCVregs[RISCV_ft0], &RISCVregs[RISCV_ft1], &RISCVregs[RISCV_ft2],  &RISCVregs[RISCV_ft3],
        &RISCVregs[RISCV_ft4], &RISCVregs[RISCV_ft5], &RISCVregs[RISCV_ft6],  &RISCVregs[RISCV_ft7],
        &RISCVregs[RISCV_ft8], &RISCVregs[RISCV_ft9], &RISCVregs[RISCV_ft10], &RISCVregs[RISCV_ft11],

        &RISCVregs[RISCV_fa0], &RISCVregs[RISCV_fa1], &RISCVregs[RISCV_fa2],  &RISCVregs[RISCV_fa3],
        &RISCVregs[RISCV_fa4], &RISCVregs[RISCV_fa5], &RISCVregs[RISCV_fa6],  &RISCVregs[RISCV_fa7],
        };
    }

    friend class RiscV64InstructionConstructor;

    RiscV64Instruction() : MachineBaseInstruction(MachineBaseInstruction::RiscV), imm(0), use_label(false) {}

public:
    void setOpcode(int op, bool use_label) {
        this->op = op;
        this->use_label = use_label;
    }
    void setRd(Register rd) { this->rd = rd; }
    void setRs1(Register rs1) { this->rs1 = rs1; }
    void setRs2(Register rs2) { this->rs2 = rs2; }
    void setRs3(Register rs3) { this->rs3 = rs3; }
    void setImm(int imm) { this->imm = imm; }
    void setLabel(RiscVLabel label) { this->label = label; }
    void setCalliregNum(int n) { callireg_num = n; }
    void setCallfregNum(int n) { callfreg_num = n; }
    void setRetType(int use) { ret_type = use; }
    Register getRd() { return rd; }
    Register getRs1() { return rs1; }
    Register getRs2() { return rs2; }
    Register getRs3() { return rs3; }
    void setUseLabel(bool use_label) { this->use_label = use_label; }
    bool getUseLabel() { return use_label; }
    int getImm() { return imm; }
    RiscVLabel getLabel() { return label; }
    int getOpcode() { return op; }
    std::vector<Register *> GetReadReg();
    std::vector<Register *> GetWriteReg();
    int GetLatency() { return OpTable[op].latency; }
};

class RiscV64InstructionConstructor {
    static RiscV64InstructionConstructor instance;

    RiscV64InstructionConstructor() {}

public:
    static RiscV64InstructionConstructor *GetConstructor() { return &instance; }
    // 函数命名方法大部分与RISC-V指令格式一致

    // example: addw Rd, Rs1, Rs2
    RiscV64Instruction *ConstructR(int op, Register Rd, Register Rs1, Register Rs2) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, false);
        Assert(OpTable[op].ins_formattype == RvOpInfo::R_type);
        ret->setRd(Rd);
        ret->setRs1(Rs1);
        ret->setRs2(Rs2);
        return ret;
    }
    // example: fmv.x.w Rd, Rs1
    RiscV64Instruction *ConstructR2(int op, Register Rd, Register Rs1) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, false);
        Assert(OpTable[op].ins_formattype == RvOpInfo::R2_type);
        ret->setRd(Rd);
        ret->setRs1(Rs1);
        return ret;
    }
    // example: fmadd.s Rd, Rs1, Rs2, Rs3
    RiscV64Instruction *ConstructR4(int op, Register Rd, Register Rs1, Register Rs2, Register Rs3) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, false);
        Assert(OpTable[op].ins_formattype == RvOpInfo::R4_type);
        ret->setRd(Rd);
        ret->setRs1(Rs1);
        ret->setRs2(Rs2);
        ret->setRs3(Rs3);
        return ret;
    }
    // example: lw Rd, imm(Rs1)
    // example: addi Rd, Rs1, imm
    RiscV64Instruction *ConstructIImm(int op, Register Rd, Register Rs1, int imm) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, false);
        Assert(OpTable[op].ins_formattype == RvOpInfo::I_type);
        ret->setRd(Rd);
        ret->setRs1(Rs1);
        ret->setImm(imm);
        return ret;
    }
    // example: lw Rd label(Rs1)   =>  lw Rd %lo(label_name)(Rs1)
    // example: addi Rd, Rs1, label  =>  addi Rd, Rs1, %lo(label_name)
    RiscV64Instruction *ConstructILabel(int op, Register Rd, Register Rs1, RiscVLabel label) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, true);
        Assert(OpTable[op].ins_formattype == RvOpInfo::I_type);
        ret->setRd(Rd);
        ret->setRs1(Rs1);
        ret->setLabel(label);
        return ret;
    }
    // example: sw value imm(ptr)
    RiscV64Instruction *ConstructSImm(int op, Register value, Register ptr, int imm) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, false);
        Assert(OpTable[op].ins_formattype == RvOpInfo::S_type);
        ret->setRs1(value);
        ret->setRs2(ptr);
        ret->setImm(imm);
        return ret;
    }
    // example: sw value label(ptr)  =>  sw value %lo(label_name)(ptr)
    RiscV64Instruction *ConstructSLabel(int op, Register value, Register ptr, RiscVLabel label) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, true);
        Assert(OpTable[op].ins_formattype == RvOpInfo::S_type);
        ret->setRs1(value);
        ret->setRs2(ptr);
        ret->setLabel(label);
        return ret;
    }
    // example: b(cond) Rs1, Rs2,label  =>  bne Rs1, Rs2, .L3(标签具体如何输出见riscv64_printasm.cc)
    RiscV64Instruction *ConstructBLabel(int op, Register Rs1, Register Rs2, RiscVLabel label) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, true);
        Assert(OpTable[op].ins_formattype == RvOpInfo::B_type);
        ret->setRs1(Rs1);
        ret->setRs2(Rs2);
        ret->setLabel(label);
        return ret;
    }
    // example: lui Rd, imm
    RiscV64Instruction *ConstructUImm(int op, Register Rd, int imm) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, false);
        Assert(OpTable[op].ins_formattype == RvOpInfo::U_type);
        ret->setRd(Rd);
        ret->setImm(imm);
        return ret;
    }
    // example: lui Rd, %hi(label_name)
    RiscV64Instruction *ConstructULabel(int op, Register Rd, RiscVLabel label) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, true);
        Assert(OpTable[op].ins_formattype == RvOpInfo::U_type);
        ret->setRd(Rd);
        ret->setLabel(label);
        return ret;
    }
    // example: jal rd, label  =>  jal a0, .L4
    RiscV64Instruction *ConstructJLabel(int op, Register rd, RiscVLabel label) {
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, true);
        Assert(OpTable[op].ins_formattype == RvOpInfo::J_type);
        ret->setRd(rd);
        ret->setLabel(label);
        return ret;
    }
    // example: call funcname
    // iregnum 和 fregnum 表示该函数调用会分别用几个物理寄存器和浮点寄存器传参
    // iregnum 和 fregnum 的作用为精确确定call会读取哪些寄存器 (具体见GetCall_typeWritereg()函数)
    // 可以进行更精确的寄存器分配
    // 对于函数调用，我们单独处理这一条指令，而不是用真指令替代，原因是函数调用涉及到部分寄存器的读写
    RiscV64Instruction *ConstructCall(int op, std::string funcname, int iregnum, int fregnum) {
        Assert(OpTable[op].ins_formattype == RvOpInfo::CALL_type);
        RiscV64Instruction *ret = new RiscV64Instruction();
        ret->setOpcode(op, true);
        // ret->setRd(GetPhysicalReg(phy_rd));
        ret->setCalliregNum(iregnum);
        ret->setCallfregNum(fregnum);
        ret->setLabel(RiscVLabel(funcname, false));
        return ret;
    }
};
extern RiscV64InstructionConstructor *rvconstructor;

#include "../common/machine_instruction_structures/machine.h"
class RiscV64Function;
class RiscV64Unit;

class RiscV64Block : public MachineBlock {
public:
    RiscV64Block(int id) : MachineBlock(id) {}
};

class RiscV64BlockFactory : public MachineBlockFactory {
public:
    MachineBlock *CreateBlock(int id) { return new RiscV64Block(id); }
};

class RiscV64Function : public MachineFunction {
public:
    RiscV64Function(std::string name) : MachineFunction(name, new RiscV64BlockFactory()) {}

private:
    // TODO: add your own members here
public:
    // TODO: add your own members here
};
class RiscV64Unit : public MachineUnit {};

class RiscV64RegisterAllocTools : public PhysicalRegistersAllocTools {
protected:
    std::vector<int> getValidRegs(LiveInterval interval);
    std::vector<int> getAliasRegs(int phy_id) { return std::vector<int>({phy_id}); }

public:
    RiscV64RegisterAllocTools() { phy_occupied.resize(64); }
    void clear() {
        phy_occupied.clear();
        Assert(phy_occupied.empty());
        phy_occupied.resize(64);
        mem_occupied.clear();
        Assert(mem_occupied.empty());
    }
};

class RiscV64Spiller : public SpillCodeGen {
private:
    // 生成从栈中读取溢出寄存器的指令
    Register GenerateReadCode(std::list<MachineBaseInstruction *>::iterator &it, int raw_stk_offset,
                              MachineDataType type);
    // 生成将溢出寄存器写入栈的指令
    Register GenerateWriteCode(std::list<MachineBaseInstruction *>::iterator &it, int raw_stk_offset,
                               MachineDataType type);
};

#endif