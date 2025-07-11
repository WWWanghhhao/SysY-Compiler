#ifndef SYSY_TREE_H
#define SYSY_TREE_H

// AST definition
#include "Instruction.h"
#include "symtab.h"
#include "tree.h"
#include <iostream>
#include <vector>

// TODO(): 在语义分析，类型检查和中间代码生成阶段加入更多你需要的成员变量

// 请注意代码中的typedef，为了方便书写，将一些类的指针进行了重命名, 如果不习惯该种风格，可以自行修改
// (修改后记得修改SysY_parser.y中的定义)

// exp basic_class
class __Expression : public tree_node {
public:
    int true_label = -1;
    int false_label = -1;
};
typedef __Expression *Expression;

// AddExp
class Exp : public __Expression {
public:
    Expression addexp;
    Exp(Expression add) : addexp(add) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// AddExp + MulExp
class AddExp_plus : public __Expression {
public:
    Expression addexp;
    Expression mulexp;
    AddExp_plus(Expression add, Expression mul) : addexp(add), mulexp(mul) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// AddExp - MulExp
class AddExp_sub : public __Expression {
public:
    Expression addexp;
    Expression mulexp;
    AddExp_sub(Expression add, Expression mul) : addexp(add), mulexp(mul) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// MulExp * UnaryExp
class MulExp_mul : public __Expression {
public:
    Expression mulexp;
    Expression unary_exp;
    MulExp_mul(Expression mul, Expression unary) : mulexp(mul), unary_exp(unary) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// MulExp / UnaryExp
class MulExp_div : public __Expression {
public:
    Expression mulexp;
    Expression unary_exp;
    MulExp_div(Expression mul, Expression unary) : mulexp(mul), unary_exp(unary) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// MulExp % UnaryExp
class MulExp_mod : public __Expression {
public:
    Expression mulexp;
    Expression unary_exp;
    MulExp_mod(Expression mul, Expression unary) : mulexp(mul), unary_exp(unary) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// RelExp <= AddExp
class RelExp_leq : public __Expression {
public:
    Expression relexp;
    Expression addexp;
    // add constructor
    RelExp_leq(Expression relexp, Expression addexp) : relexp(relexp), addexp(addexp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// RelExp < AddExp
class RelExp_lt : public __Expression {
public:
    Expression relexp;
    Expression addexp;
    RelExp_lt(Expression relexp, Expression addexp) : relexp(relexp), addexp(addexp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// RelExp >= AddExp
class RelExp_geq : public __Expression {
public:
    Expression relexp;
    Expression addexp;
    RelExp_geq(Expression relexp, Expression addexp) : relexp(relexp), addexp(addexp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// RelExp > AddExp
class RelExp_gt : public __Expression {
public:
    Expression relexp;
    Expression addexp;
    RelExp_gt(Expression relexp, Expression addexp) : relexp(relexp), addexp(addexp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// EqExp == RelExp
class EqExp_eq : public __Expression {
public:
    Expression eqexp;
    Expression relexp;
    EqExp_eq(Expression eqexp, Expression relexp) : eqexp(eqexp), relexp(relexp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// EqExp != RelExp
class EqExp_neq : public __Expression {
public:
    Expression eqexp;
    Expression relexp;
    EqExp_neq(Expression eqexp, Expression relexp) : eqexp(eqexp), relexp(relexp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// LAndExp && EqExp
class LAndExp_and : public __Expression {
public:
    Expression landexp;
    Expression eqexp;
    LAndExp_and(Expression landexp, Expression eqexp) : landexp(landexp), eqexp(eqexp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// LOrExp || LAndExp
class LOrExp_or : public __Expression {
public:
    Expression lorexp;
    Expression landexp;
    ;
    LOrExp_or(Expression lorexp, Expression landexp) : lorexp(lorexp), landexp(landexp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// AddExp
class ConstExp : public __Expression {
public:
    Expression addexp;
    ConstExp(Expression addexp) : addexp(addexp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// lval   name{[dim]}   eg. a[4][5+4][3+4*9]
class Lval : public __Expression {
public:
    Symbol name;
    std::vector<Expression> *dims;
    // 如果dims为nullptr, 表示该变量不含数组下标, 你也可以通过其他方式判断，但需要修改SysY_parser.y已有的代码

    int scope = -1;    // 在语义分析阶段填入正确的作用域
    Lval(Symbol n, std::vector<Expression> *d) : name(n), dims(d) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

//{Exp,Exp,Exp,Exp}
class FuncRParams : public __Expression {
public:
    std::vector<Expression> *params{};
    FuncRParams(std::vector<Expression> *p) : params(p) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// name(FuncRParams)
class Func_call : public __Expression {
public:
    Symbol name;
    Expression funcr_params;
    Func_call(Symbol n, Expression f) : name(n), funcr_params(f) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// + UnaryExp
class UnaryExp_plus : public __Expression {
public:
    Expression unary_exp;
    UnaryExp_plus(Expression unary_exp) : unary_exp(unary_exp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// - UnaryExp
class UnaryExp_neg : public __Expression {
public:
    Expression unary_exp;
    UnaryExp_neg(Expression unary_exp) : unary_exp(unary_exp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// ! UnaryExp
class UnaryExp_not : public __Expression {
public:
    Expression unary_exp;
    UnaryExp_not(Expression unary_exp) : unary_exp(unary_exp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class IntConst : public __Expression {
public:
    int val;
    IntConst(int v) : val(v) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class FloatConst : public __Expression {
public:
    float val;
    FloatConst(float v) : val(v) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class StringConst : public __Expression {
public:
    Symbol str;
    StringConst(Symbol s) : str(s) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

//( Exp )
class PrimaryExp_branch : public __Expression {
public:
    Expression exp;
    PrimaryExp_branch(Expression exp) : exp(exp) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class __Block;
typedef __Block *Block;
// stmt basic_class
class __Stmt : public tree_node {
public:
};
typedef __Stmt *Stmt;

class null_stmt : public __Stmt {
public:
    void codeIR() {}
    void TypeCheck() {}
    void printAST(std::ostream &s, int pad);
};

// lval = exp;
class assign_stmt : public __Stmt {
public:
    Expression lval;
    Expression exp;
    // construction
    assign_stmt(Expression l, Expression e) : lval(l), exp(e) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// exp;
class expr_stmt : public __Stmt {
public:
    Expression exp;
    expr_stmt(Expression e) : exp(e) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// block
class block_stmt : public __Stmt {
public:
    Block b;
    block_stmt(Block b) : b(b) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class ifelse_stmt : public __Stmt {
public:
    Expression Cond;
    Stmt ifstmt;
    Stmt elsestmt;    // else
    // construction
    ifelse_stmt(Expression c, Stmt i, Stmt t) : Cond(c), ifstmt(i), elsestmt(t) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// only if
class if_stmt : public __Stmt {
public:
    Expression Cond;
    Stmt ifstmt;
    // construction
    if_stmt(Expression c, Stmt i) : Cond(c), ifstmt(i) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class while_stmt : public __Stmt {
public:
    Expression Cond;
    Stmt body;
    // construction
    while_stmt(Expression c, Stmt b) : Cond(c), body(b) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// continue;
class continue_stmt : public __Stmt {
public:
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// break;
class break_stmt : public __Stmt {
public:
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// return return_exp;
class return_stmt : public __Stmt {
public:
    Expression return_exp;
    return_stmt(Expression r) : return_exp(r) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class return_stmt_void : public __Stmt {
public:
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};
class __Decl;
typedef __Decl *Decl;

class __InitVal : public tree_node {
public:
    virtual std::vector<__InitVal *> *GetInitValArray() = 0;
};
typedef __InitVal *InitVal;

// InitVal -> {InitVal,InitVal,InitVal,...}
// eg. {2,3,{4,5,6},{3,{4,5,6,{3,5}}}}
class ConstInitVal : public __InitVal {
public:
    std::vector<InitVal> *initval;
    ConstInitVal(std::vector<InitVal> *i) : initval(i) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);

    std::vector<__InitVal *> *GetInitValArray() { return initval; }
};

class ConstInitVal_exp : public __InitVal {
public:
    Expression exp;
    ConstInitVal_exp(Expression e) : exp(e) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);

    std::vector<__InitVal *> *GetInitValArray() { return nullptr; }
};

// InitVal -> {InitVal,InitVal,InitVal,...}
class VarInitVal : public __InitVal {
public:
    std::vector<InitVal> *initval;
    VarInitVal(std::vector<InitVal> *i) : initval(i) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
    // 000
    std::vector<__InitVal *> *GetInitValArray() { return initval; }
};

class VarInitVal_exp : public __InitVal {
public:
    Expression exp;
    VarInitVal_exp(Expression e) : exp(e) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);

    std::vector<__InitVal *> *GetInitValArray() { return nullptr; }
};

class __Def : public tree_node {
public:
    int scope = -1;                                    // 在语义分析阶段填入正确的作用域
    virtual Symbol GetName() = 0;                      // 虚函数：获取命名
    virtual std::vector<Expression> *GetDims() = 0;    // 虚函数：获取维度信息
    virtual InitVal GetInitVal() = 0;                  // 虚函数：获取初始值
};
typedef __Def *Def;

class VarDef_no_init : public __Def {
public:
    Symbol name;
    std::vector<Expression> *dims;
    // 如果dims为nullptr, 表示该变量不含数组下标, 你也可以通过其他方式判断，但需要修改SysY_parser.y已有的代码
    VarDef_no_init(Symbol n, std::vector<Expression> *d) : name(n), dims(d) {}

    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);

    // 虚函数在子类中的具体实现
    Symbol GetName() { return name; }
    std::vector<Expression> *GetDims() { return dims; }
    InitVal GetInitVal() { return nullptr; }
};

class VarDef : public __Def {
public:
    Symbol name;
    std::vector<Expression> *dims;
    // 如果dims为nullptr, 表示该变量不含数组下标, 你也可以通过其他方式判断，但需要修改SysY_parser.y已有的代码
    InitVal init;
    VarDef(Symbol n, std::vector<Expression> *d, InitVal i) : name(n), dims(d), init(i) {}

    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);

    // 虚函数在子类中的具体实现
    Symbol GetName() { return name; }
    std::vector<Expression> *GetDims() { return dims; }
    InitVal GetInitVal() { return init; }
};

class ConstDef : public __Def {
public:
    Symbol name;
    std::vector<Expression> *dims;
    // 如果dims为nullptr, 表示该变量不含数组下标, 你也可以通过其他方式判断，但需要修改SysY_parser.y已有的代码
    InitVal init;
    ConstDef(Symbol n, std::vector<Expression> *d, InitVal i) : name(n), dims(d), init(i) {}

    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);

    // 虚函数在子类中的具体实现
    Symbol GetName() { return name; }
    std::vector<Expression> *GetDims() { return dims; }
    InitVal GetInitVal() { return init; }
};

// decl basic_class
class __Decl : public tree_node {
public:
};

// var definition
class VarDecl : public __Decl {
public:
    Type::ty type_decl;
    std::vector<Def> *var_def_list{};
    // construction
    VarDecl(Type::ty t, std::vector<Def> *v) : type_decl(t), var_def_list(v) {}

    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// const var definition
class ConstDecl : public __Decl {
public:
    Type::ty type_decl;
    std::vector<Def> *var_def_list{};
    // construction
    ConstDecl(Type::ty t, std::vector<Def> *v) : type_decl(t), var_def_list(v) {}

    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class __BlockItem : public tree_node {
public:
};
typedef __BlockItem *BlockItem;

class BlockItem_Decl : public __BlockItem {
public:
    Decl decl;
    BlockItem_Decl(Decl d) : decl(d) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class BlockItem_Stmt : public __BlockItem {
public:
    Stmt stmt;
    BlockItem_Stmt(Stmt s) : stmt(s) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// block
class __Block : public tree_node {
public:
    std::vector<BlockItem> *item_list{};
    // construction
    __Block() {}
    __Block(std::vector<BlockItem> *i) : item_list(i) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

// FuncParam -> Type IDENT
// FuncParam -> Type IDENT [] {[Exp]}
class __FuncFParam : public tree_node {
public:
    Type::ty type_decl;
    std::vector<Expression> *dims;
    // 如果dims为nullptr, 表示该变量不含数组下标, 你也可以通过其他方式判断，但需要修改SysY_parser.y已有的代码
    Symbol name;
    int scope = -1;    // 在语义分析阶段填入正确的作用域

    __FuncFParam(Type::ty t, Symbol n, std::vector<Expression> *d) {
        type_decl = t;
        name = n;
        dims = d;
    }
    __FuncFParam(Type::ty t, std::vector<Expression> *d) {
        type_decl = t;
        dims = d;
    }
    __FuncFParam(Type::ty t) : type_decl(t) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};
typedef __FuncFParam *FuncFParam;

// return_type name '(' [formals] ')' block
class __FuncDef : public tree_node {
public:
    Type::ty return_type;
    Symbol name;
    std::vector<FuncFParam> *formals;
    Block block;
    __FuncDef(Type::ty t, Symbol functionName, std::vector<FuncFParam> *f, Block b) {
        formals = f;
        name = functionName;
        return_type = t;
        block = b;
    }
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};
typedef __FuncDef *FuncDef;

class __CompUnit : public tree_node {
public:
};
typedef __CompUnit *CompUnit;

class CompUnit_Decl : public __CompUnit {
public:
    Decl decl;
    CompUnit_Decl(Decl d) : decl(d) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class CompUnit_FuncDef : public __CompUnit {
public:
    FuncDef func_def;
    CompUnit_FuncDef(FuncDef f) : func_def(f) {}
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};

class __Program : public tree_node {
public:
    std::vector<CompUnit> *comp_list;

    __Program(std::vector<CompUnit> *c) { comp_list = c; }
    void codeIR();
    void TypeCheck();
    void printAST(std::ostream &s, int pad);
};
typedef __Program *Program;
#endif