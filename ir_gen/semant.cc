#include "semant.h"
#include "../include/SysY_tree.h"
#include "../include/ir.h"
#include "../include/type.h"
#include <algorithm>
#include <codecvt>
#include <csignal>
#include <cstddef>
#include <iostream>
#include <string>
#include <vector>
/*
    语义分析阶段需要对语法树节点上的类型和常量等信息进行标注, 即NodeAttribute类
    同时还需要标注每个变量的作用域信息，即部分语法树节点中的scope变量
    你可以在utils/ast_out.cc的输出函数中找到你需要关注哪些语法树节点中的NodeAttribute类及其他变量
    以及对语义错误的代码输出报错信息
*/

/*
    错误检查的基本要求:
    • 检查 main 函数是否存在 (根据SysY定义，如果不存在main函数应当报错)；
    • 检查未声明变量，及在同一作用域下重复声明的变量；
    • 条件判断和运算表达式：int 和 bool 隐式类型转换（例如int a=5，return a+!a）；
    • 数值运算表达式：运算数类型是否正确 (如返回值为 void 的函数调用结果是否参与了其他表达式的计算)；
    • 检查未声明函数，及函数形参是否与实参类型及数目匹配；
    • 检查是否存在整型变量除以整型常量0的情况 (如对于表达式a/(5-4-1)，编译器应当给出警告或者直接报错)；

    错误检查的进阶要求:
    • 对数组维度进行相应的类型检查 (例如维度是否有浮点数，定义维度时是否为常量等)；
    • 对float进行隐式类型转换以及其他float相关的检查 (例如模运算中是否有浮点类型变量等)；
*/

// 调试函数
void zHere() { std::cout << "   here\n"; }

// 记录 while 嵌套次数
int WhileCount = 0;

// 记录是否有 main 函数
bool haveMain = false;

extern LLVMIR llvmIR;

SemantTable semant_table;
std::vector<std::string> error_msgs{};    // 将语义错误信息保存到该变量中

// 常量数组初始化
size_t fillIntArray(std::vector<InitVal> *currentInit, std::vector<int> &array, const std::vector<int> &dims,
                    size_t index, int depth, bool constTag) {
    if (!currentInit) {
        return index;
    }
    if (depth >= dims.size()) {
        error_msgs.push_back("Initial values exceed array dimensions in line " +
                             std::to_string((*currentInit)[0]->GetLineNumber()) + "\n");
        return index;
    }

    // Precompute elementsInDimension and next_index
    size_t elementsInDimension = 1;
    for (int i = depth; i < dims.size(); ++i) {
        elementsInDimension *= dims[i];
    }
    int next_index = elementsInDimension + index;

    // Avoid recalculating elementsInDimension and next_index inside the loop
    for (auto val : *currentInit) {
        if (index >= array.size()) {
            error_msgs.push_back("Initial values exceed array dimensions in line " +
                                 std::to_string(val->GetLineNumber()) + "\n");
            break;
        }

        InitVal dym;
        bool isConst = constTag;

        // Perform dynamic cast only once per iteration
        if (isConst) {
            dym = dynamic_cast<ConstInitVal *>(val);
        } else {
            dym = dynamic_cast<VarInitVal *>(val);
        }

        if (!dym) {
            if (val->attribute.T.type != Type::INT) {
                error_msgs.push_back("Initial of Int Array is not Int type in line " +
                                     std::to_string(val->GetLineNumber()) + "\n");
            }

            if (array[index] != 0) {
                error_msgs.push_back("Initial values exceed array dimensions in line " +
                                     std::to_string(val->GetLineNumber()) + "\n");
                break;
            }
            array[index++] = val->attribute.V.val.IntVal;

            if (index - 1 >= next_index) {
                error_msgs.push_back("Initial values exceed array dimensions in line " +
                                     std::to_string(val->GetLineNumber()) + "\n");
            }
        } else {
            index = fillIntArray(dym->GetInitValArray(), array, dims, index, 1 + depth, constTag);
        }
    }

    return next_index;
}

size_t fillFloatArray(std::vector<InitVal> *currentInit, std::vector<float> &array, const std::vector<int> dims,
                      size_t index, int depth, bool constTag) {
    if (!currentInit) {
        return index;
    }
    if (depth >= dims.size()) {
        error_msgs.push_back("Initial values exceed array dimensions in line " +
                             std::to_string((*currentInit)[0]->GetLineNumber()) + "\n");
        return index;
    }
    size_t elementsInDimension = 1;
    for (int i = depth; i < dims.size(); ++i) {
        elementsInDimension *= dims[i];
    }
    int next_index = elementsInDimension + index;

    for (auto val : *currentInit) {
        if (index >= array.size()) {
            error_msgs.push_back("Initial values exceed array dimensions in line " +
                                 std::to_string(val->GetLineNumber()) + "\n");
        }
        InitVal dym;
        if (constTag) {
            dym = dynamic_cast<ConstInitVal *>(val);
        } else {
            dym = dynamic_cast<VarInitVal *>(val);
        }

        if (!dym) {
            if (val->attribute.T.type == Type::FLOAT) {
                if (index >= array.size()) {
                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                         std::to_string(val->GetLineNumber()) + "\n");
                    break;
                }
                if (array[index] != 0) {
                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                         std::to_string(val->GetLineNumber()) + "\n");
                    break;
                }
                array[index++] = val->attribute.V.val.FloatVal;

                if (index - 1 >= next_index) {
                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                         std::to_string(val->GetLineNumber()) + "\n");
                }

            } else if (val->attribute.T.type == Type::INT) {
                if (index >= array.size()) {
                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                         std::to_string(val->GetLineNumber()) + "\n");
                    break;
                }
                if (array[index] != 0) {
                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                         std::to_string(val->GetLineNumber()) + "\n");
                    break;
                }
                array[index++] = float(val->attribute.V.val.IntVal);

                if (index - 1 >= next_index) {
                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                         std::to_string(val->GetLineNumber()) + "\n");
                }
            } else {
                error_msgs.push_back("Initial of Float Array is not Float or Int type in line " +
                                     std::to_string(val->GetLineNumber()) + "\n");
            }

        } else {
            index = fillFloatArray(dym->GetInitValArray(), array, dims, index, 1 + depth, constTag);
        }
    }

    return next_index;
}

// 根据索引访问展平数组
int getIntArrayVal(VarAttribute val, std::vector<int> index) {
    if (index.size() == 1) {
        // 一维数组，直接返回值
        return val.IntInitVals[index[0]];
    }
    int idx = index[0];
    for (size_t i = 0; i < index.size(); i++) {
        // 将当前索引与之前计算的索引相加，同时乘以对应的维度大小
        idx *= val.dims[i];
        idx += index[i];
    }
    return val.IntInitVals[idx];
}

int getFloatArrayVal(VarAttribute val, std::vector<int> index) {
    if (index.size() == 1) {
        // 一维数组，直接返回值
        return val.FloatInitVals[index[0]];
    }
    int idx = index[0];
    for (size_t i = 0; i < index.size(); i++) {
        // 将当前索引与之前计算的索引相加，同时乘以对应的维度大小
        idx *= val.dims[i];
        idx += index[i];
    }
    return val.FloatInitVals[idx];
}

void __Program::TypeCheck() {
    semant_table.symbol_table.enter_scope();
    auto comp_vector = *comp_list;
    for (auto comp : comp_vector) {
        comp->TypeCheck();
    }

    if (!haveMain) {
        error_msgs.push_back("Don't have main function! \n");
    }
}

void Exp::TypeCheck() {
    addexp->TypeCheck();

    attribute = addexp->attribute;
}

const Type::ty GetCorrectType[5][5]{{Type::VOID, Type::VOID, Type::VOID, Type::VOID, Type::VOID},
                                    {Type::VOID, Type::INT, Type::FLOAT, Type::INT, Type::VOID},
                                    {Type::VOID, Type::FLOAT, Type::FLOAT, Type::FLOAT, Type::VOID},
                                    {Type::VOID, Type::INT, Type::FLOAT, Type::BOOL, Type::VOID},
                                    {Type::VOID, Type::VOID, Type::VOID, Type::VOID, Type::VOID}};

void AddExp_plus::TypeCheck() {
    addexp->TypeCheck();
    mulexp->TypeCheck();

    // TODO("BinaryExp Semant");

    Type::ty t = GetCorrectType[addexp->attribute.T.type][mulexp->attribute.T.type];
    attribute.V.ConstTag = addexp->attribute.V.ConstTag & mulexp->attribute.V.ConstTag;
    attribute.T.type = t;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (addexp->attribute.T.type == mulexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.IntVal = addexp->attribute.V.val.IntVal + mulexp->attribute.V.val.IntVal;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.IntVal = int(addexp->attribute.V.val.BoolVal) + mulexp->attribute.V.val.IntVal;
        } else {
            // 另一个是 BOOL
            attribute.V.val.IntVal = addexp->attribute.V.val.IntVal + int(mulexp->attribute.V.val.BoolVal);
        }
    } else if (t == Type::FLOAT) {
        if (addexp->attribute.T.type == mulexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.FloatVal = addexp->attribute.V.val.FloatVal + mulexp->attribute.V.val.FloatVal;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.FloatVal = float(addexp->attribute.V.val.BoolVal) + mulexp->attribute.V.val.FloatVal;
        } else if (addexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.FloatVal = float(addexp->attribute.V.val.IntVal) + mulexp->attribute.V.val.FloatVal;
        } else if (mulexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.FloatVal = float(mulexp->attribute.V.val.BoolVal) + addexp->attribute.V.val.FloatVal;
        } else {
            // 另一个是 INT
            attribute.V.val.FloatVal = float(mulexp->attribute.V.val.IntVal) + addexp->attribute.V.val.FloatVal;
        }
    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = addexp->attribute.V.val.BoolVal || mulexp->attribute.V.val.BoolVal;
        // BOOL 类型的加法：或运算
    }
}

void AddExp_sub::TypeCheck() {
    addexp->TypeCheck();
    mulexp->TypeCheck();

    // TODO("BinaryExp Semant");

    Type::ty t = GetCorrectType[addexp->attribute.T.type][mulexp->attribute.T.type];
    attribute.V.ConstTag = addexp->attribute.V.ConstTag & mulexp->attribute.V.ConstTag;
    attribute.T.type = t;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (addexp->attribute.T.type == mulexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.IntVal = addexp->attribute.V.val.IntVal - mulexp->attribute.V.val.IntVal;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.IntVal = int(addexp->attribute.V.val.BoolVal) - mulexp->attribute.V.val.IntVal;
        } else {
            // 另一个是 BOOL
            attribute.V.val.IntVal = addexp->attribute.V.val.IntVal - int(mulexp->attribute.V.val.BoolVal);
        }
    } else if (t == Type::FLOAT) {
        if (addexp->attribute.T.type == mulexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.FloatVal = addexp->attribute.V.val.FloatVal - mulexp->attribute.V.val.FloatVal;
        } else if (addexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.FloatVal = float(addexp->attribute.V.val.BoolVal) - mulexp->attribute.V.val.FloatVal;
        } else if (addexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.FloatVal = float(addexp->attribute.V.val.IntVal) - mulexp->attribute.V.val.FloatVal;
        } else if (mulexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.FloatVal = addexp->attribute.V.val.FloatVal - float(mulexp->attribute.V.val.BoolVal);
        } else {
            // 另一个是 INT
            attribute.V.val.FloatVal = addexp->attribute.V.val.FloatVal - float(mulexp->attribute.V.val.IntVal);
        }
    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = addexp->attribute.V.val.BoolVal != mulexp->attribute.V.val.BoolVal;
        // BOOL 类型的减法：相同为 false 相异为 true
    }
}

void MulExp_mul::TypeCheck() {
    mulexp->TypeCheck();
    unary_exp->TypeCheck();

    // TODO("BinaryExp Semant");

    Type::ty t = GetCorrectType[mulexp->attribute.T.type][unary_exp->attribute.T.type];
    attribute.V.ConstTag = mulexp->attribute.V.ConstTag & unary_exp->attribute.V.ConstTag;
    attribute.T.type = t;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (mulexp->attribute.T.type == unary_exp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.IntVal = mulexp->attribute.V.val.IntVal * unary_exp->attribute.V.val.IntVal;
        } else if (mulexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.IntVal = int(mulexp->attribute.V.val.BoolVal) * unary_exp->attribute.V.val.IntVal;
        } else {
            // 另一个是 BOOL
            attribute.V.val.IntVal = mulexp->attribute.V.val.IntVal * int(unary_exp->attribute.V.val.BoolVal);
        }
    } else if (t == Type::FLOAT) {
        if (mulexp->attribute.T.type == unary_exp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.FloatVal = mulexp->attribute.V.val.FloatVal * unary_exp->attribute.V.val.FloatVal;
        } else if (mulexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.FloatVal = float(mulexp->attribute.V.val.BoolVal) * unary_exp->attribute.V.val.FloatVal;
        } else if (mulexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.FloatVal = float(mulexp->attribute.V.val.IntVal) * unary_exp->attribute.V.val.FloatVal;
        } else if (unary_exp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.FloatVal = float(unary_exp->attribute.V.val.BoolVal) * mulexp->attribute.V.val.FloatVal;
        } else {
            // 另一个是 INT
            attribute.V.val.FloatVal = float(unary_exp->attribute.V.val.IntVal) * mulexp->attribute.V.val.FloatVal;
        }
    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = mulexp->attribute.V.val.BoolVal & unary_exp->attribute.V.val.BoolVal;
        // BOOL 类型的乘法：与操作
    }
}

void MulExp_div::TypeCheck() {
    mulexp->TypeCheck();
    unary_exp->TypeCheck();

    // TODO("BinaryExp Semant");

    if (unary_exp->attribute.T.type == Type::BOOL) {
        if (unary_exp->attribute.V.ConstTag && unary_exp->attribute.V.val.BoolVal == false) {
            error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
        }
    } else if (unary_exp->attribute.T.type == Type::INT) {
        if (unary_exp->attribute.V.ConstTag && unary_exp->attribute.V.val.IntVal == 0) {
            error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
        }
    } else if (unary_exp->attribute.T.type == Type::FLOAT) {
        if (unary_exp->attribute.V.ConstTag && unary_exp->attribute.V.val.FloatVal == 0) {
            error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
        }
    }

    Type::ty t = GetCorrectType[mulexp->attribute.T.type][unary_exp->attribute.T.type];
    attribute.V.ConstTag = mulexp->attribute.V.ConstTag & unary_exp->attribute.V.ConstTag;
    attribute.T.type = t;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (mulexp->attribute.T.type == unary_exp->attribute.T.type) {
            // 两个操作数类型相同
            if (unary_exp->attribute.V.val.IntVal != 0) {
                attribute.V.val.IntVal = mulexp->attribute.V.val.IntVal / unary_exp->attribute.V.val.IntVal;
            } else {
                error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
            }

        } else if (mulexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            if (unary_exp->attribute.V.val.IntVal != 0) {
                attribute.V.val.IntVal = int(mulexp->attribute.V.val.BoolVal) / unary_exp->attribute.V.val.IntVal;
            } else {
                error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
            }

        } else {
            // 另一个是 BOOL
            if (unary_exp->attribute.V.val.BoolVal != false) {
                attribute.V.val.IntVal = mulexp->attribute.V.val.IntVal / int(unary_exp->attribute.V.val.BoolVal);
            } else {
                error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
            }
        }

    } else if (t == Type::FLOAT) {
        if (mulexp->attribute.T.type == unary_exp->attribute.T.type) {
            // 两个操作数类型相同
            if (unary_exp->attribute.V.val.FloatVal != 0) {
                attribute.V.val.FloatVal = mulexp->attribute.V.val.FloatVal / unary_exp->attribute.V.val.FloatVal;
            } else {
                error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
            }

        } else if (mulexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            if (unary_exp->attribute.V.val.FloatVal != 0) {
                attribute.V.val.FloatVal = float(mulexp->attribute.V.val.BoolVal) / unary_exp->attribute.V.val.FloatVal;
            } else {
                error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
            }

        } else if (mulexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            if (unary_exp->attribute.V.val.FloatVal != 0) {
                attribute.V.val.FloatVal = float(mulexp->attribute.V.val.IntVal) / unary_exp->attribute.V.val.FloatVal;
            } else {
                error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
            }

        } else if (unary_exp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            if (unary_exp->attribute.V.val.BoolVal != false) {
                attribute.V.val.FloatVal = mulexp->attribute.V.val.FloatVal / float(unary_exp->attribute.V.val.BoolVal);
            } else {
                error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
            }

        } else {
            // 另一个是 INT
            if (unary_exp->attribute.V.val.IntVal != 0) {
                attribute.V.val.FloatVal = mulexp->attribute.V.val.FloatVal / float(unary_exp->attribute.V.val.IntVal);
            } else {
                error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
            }
        }

    } else if (t == Type::BOOL) {
        if (unary_exp->attribute.V.val.BoolVal != false) {
            attribute.V.val.BoolVal = mulexp->attribute.V.val.BoolVal;
        } else {
            error_msgs.push_back("Div 0 in line " + std::to_string(line_number) + "\n");
        }
        // BOOL 类型的除法
    }
}

void MulExp_mod::TypeCheck() {
    mulexp->TypeCheck();
    unary_exp->TypeCheck();

    // TODO("BinaryExp Semant");

    Type::ty t = GetCorrectType[mulexp->attribute.T.type][unary_exp->attribute.T.type];
    attribute.V.ConstTag = mulexp->attribute.V.ConstTag & unary_exp->attribute.V.ConstTag;
    attribute.T.type = t;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (mulexp->attribute.T.type == unary_exp->attribute.T.type) {
            // 两个操作数类型相同
            if (unary_exp->attribute.V.val.IntVal != 0) {
                attribute.V.val.IntVal = mulexp->attribute.V.val.IntVal % unary_exp->attribute.V.val.IntVal;
            } else {
                error_msgs.push_back("Mod 0 in line " + std::to_string(line_number) + "\n");
            }

        } else if (mulexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            if (unary_exp->attribute.V.val.IntVal != 0) {
                attribute.V.val.IntVal = int(mulexp->attribute.V.val.BoolVal) % unary_exp->attribute.V.val.IntVal;
            } else {
                error_msgs.push_back("Mod 0 in line " + std::to_string(line_number) + "\n");
            }

        } else {
            // 另一个是 BOOL
            if (unary_exp->attribute.V.val.BoolVal != false) {
                // attribute.V.val.IntVal = mulexp->attribute.V.val.IntVal % int(unary_exp->attribute.V.val.BoolVal);
                attribute.V.val.IntVal = 0;    // 任何数模 1 都是 0
            } else {
                error_msgs.push_back("Mod 0 in line " + std::to_string(line_number) + "\n");
            }
        }

    } else if (t == Type::FLOAT) {
        attribute.T.type = Type::VOID;
        attribute.V.ConstTag = false;
        // attribute.V.ConstTag = mulexp->attribute.V.ConstTag & unary_exp->attribute.V.ConstTag;
        error_msgs.push_back("Mod float type in line " + std::to_string(line_number) + "\n");

    } else if (t == Type::BOOL) {
        attribute.T.type = Type::INT;
        if (unary_exp->attribute.V.val.BoolVal != false) {
            attribute.V.val.IntVal = int(mulexp->attribute.V.val.BoolVal);
        } else {
            error_msgs.push_back("Mod 0 in line " + std::to_string(line_number) + "\n");
        }
        // BOOL 类型的模运算
    }
}

void RelExp_leq::TypeCheck() {
    relexp->TypeCheck();
    addexp->TypeCheck();

    // TODO("BinaryExp Semant");

    attribute.T.type = Type::BOOL;

    Type::ty t = GetCorrectType[relexp->attribute.T.type][addexp->attribute.T.type];
    attribute.V.ConstTag = relexp->attribute.V.ConstTag & addexp->attribute.V.ConstTag;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (relexp->attribute.T.type == addexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = relexp->attribute.V.val.IntVal <= addexp->attribute.V.val.IntVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = int(relexp->attribute.V.val.BoolVal) <= addexp->attribute.V.val.IntVal;

        } else {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = relexp->attribute.V.val.IntVal <= int(addexp->attribute.V.val.BoolVal);
        }

    } else if (t == Type::FLOAT) {
        if (relexp->attribute.T.type == addexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal <= addexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = float(relexp->attribute.V.val.BoolVal) <= addexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.BoolVal = float(relexp->attribute.V.val.IntVal) <= addexp->attribute.V.val.FloatVal;

        } else if (addexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal <= float(addexp->attribute.V.val.BoolVal);

        } else {
            // 另一个是 INT
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal <= float(addexp->attribute.V.val.IntVal);
        }

    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = int(relexp->attribute.V.val.BoolVal) <= int(addexp->attribute.V.val.BoolVal);
        // BOOL 类型的 <= 运算
    }
}

void RelExp_lt::TypeCheck() {
    relexp->TypeCheck();
    addexp->TypeCheck();

    // TODO("BinaryExp Semant");

    attribute.T.type = Type::BOOL;

    Type::ty t = GetCorrectType[relexp->attribute.T.type][addexp->attribute.T.type];
    attribute.V.ConstTag = relexp->attribute.V.ConstTag & addexp->attribute.V.ConstTag;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (relexp->attribute.T.type == addexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = relexp->attribute.V.val.IntVal < addexp->attribute.V.val.IntVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = int(relexp->attribute.V.val.BoolVal) < addexp->attribute.V.val.IntVal;

        } else {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = relexp->attribute.V.val.IntVal < int(addexp->attribute.V.val.BoolVal);
        }

    } else if (t == Type::FLOAT) {
        if (relexp->attribute.T.type == addexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal < addexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = float(relexp->attribute.V.val.BoolVal) < addexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.BoolVal = float(relexp->attribute.V.val.IntVal) < addexp->attribute.V.val.FloatVal;

        } else if (addexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal < float(addexp->attribute.V.val.BoolVal);

        } else {
            // 另一个是 INT
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal < float(addexp->attribute.V.val.IntVal);
        }

    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = int(relexp->attribute.V.val.BoolVal) < int(addexp->attribute.V.val.BoolVal);
        // BOOL 类型的 < 运算
    }
}

void RelExp_geq::TypeCheck() {
    relexp->TypeCheck();
    addexp->TypeCheck();

    // TODO("BinaryExp Semant");

    attribute.T.type = Type::BOOL;

    Type::ty t = GetCorrectType[relexp->attribute.T.type][addexp->attribute.T.type];
    attribute.V.ConstTag = relexp->attribute.V.ConstTag & addexp->attribute.V.ConstTag;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (relexp->attribute.T.type == addexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = relexp->attribute.V.val.IntVal >= addexp->attribute.V.val.IntVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = int(relexp->attribute.V.val.BoolVal) >= addexp->attribute.V.val.IntVal;

        } else {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = relexp->attribute.V.val.IntVal >= int(addexp->attribute.V.val.BoolVal);
        }

    } else if (t == Type::FLOAT) {
        if (relexp->attribute.T.type == addexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal >= addexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = float(relexp->attribute.V.val.BoolVal) >= addexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.BoolVal = float(relexp->attribute.V.val.IntVal) >= addexp->attribute.V.val.FloatVal;

        } else if (addexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal >= float(addexp->attribute.V.val.BoolVal);

        } else {
            // 另一个是 INT
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal >= float(addexp->attribute.V.val.IntVal);
        }

    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = int(relexp->attribute.V.val.BoolVal) >= int(addexp->attribute.V.val.BoolVal);
        // BOOL 类型的 >= 运算
    }
}

void RelExp_gt::TypeCheck() {
    relexp->TypeCheck();
    addexp->TypeCheck();

    // TODO("BinaryExp Semant");

    attribute.T.type = Type::BOOL;

    Type::ty t = GetCorrectType[relexp->attribute.T.type][addexp->attribute.T.type];
    attribute.V.ConstTag = relexp->attribute.V.ConstTag & addexp->attribute.V.ConstTag;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (relexp->attribute.T.type == addexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = relexp->attribute.V.val.IntVal > addexp->attribute.V.val.IntVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = int(relexp->attribute.V.val.BoolVal) > addexp->attribute.V.val.IntVal;

        } else {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = relexp->attribute.V.val.IntVal > int(addexp->attribute.V.val.BoolVal);
        }

    } else if (t == Type::FLOAT) {
        if (relexp->attribute.T.type == addexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal > addexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = float(relexp->attribute.V.val.BoolVal) > addexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.BoolVal = float(relexp->attribute.V.val.IntVal) > addexp->attribute.V.val.FloatVal;

        } else if (addexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal > float(addexp->attribute.V.val.BoolVal);

        } else {
            // 另一个是 INT
            attribute.V.val.BoolVal = relexp->attribute.V.val.FloatVal > float(addexp->attribute.V.val.IntVal);
        }

    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = int(relexp->attribute.V.val.BoolVal) > int(addexp->attribute.V.val.BoolVal);
        // BOOL 类型的 > 运算
    }
}

void EqExp_eq::TypeCheck() {
    eqexp->TypeCheck();
    relexp->TypeCheck();

    // TODO("BinaryExp Semant");

    attribute.T.type = Type::BOOL;

    Type::ty t = GetCorrectType[eqexp->attribute.T.type][relexp->attribute.T.type];
    attribute.V.ConstTag = eqexp->attribute.V.ConstTag & relexp->attribute.V.ConstTag;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (eqexp->attribute.T.type == relexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = eqexp->attribute.V.val.IntVal == relexp->attribute.V.val.IntVal;

        } else if (eqexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = int(eqexp->attribute.V.val.BoolVal) == relexp->attribute.V.val.IntVal;

        } else {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = eqexp->attribute.V.val.IntVal == int(relexp->attribute.V.val.BoolVal);
        }

    } else if (t == Type::FLOAT) {
        if (eqexp->attribute.T.type == relexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = eqexp->attribute.V.val.FloatVal == relexp->attribute.V.val.FloatVal;

        } else if (eqexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = float(eqexp->attribute.V.val.BoolVal) == relexp->attribute.V.val.FloatVal;

        } else if (eqexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.BoolVal = float(eqexp->attribute.V.val.IntVal) == relexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = eqexp->attribute.V.val.FloatVal == float(relexp->attribute.V.val.BoolVal);

        } else {
            // 另一个是 INT
            attribute.V.val.BoolVal = eqexp->attribute.V.val.FloatVal == float(relexp->attribute.V.val.IntVal);
        }

    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = int(eqexp->attribute.V.val.BoolVal) == int(relexp->attribute.V.val.BoolVal);
        // BOOL 类型的 == 运算
    }
}

void EqExp_neq::TypeCheck() {
    eqexp->TypeCheck();
    relexp->TypeCheck();

    // TODO("BinaryExp Semant");

    attribute.T.type = Type::BOOL;

    Type::ty t = GetCorrectType[eqexp->attribute.T.type][relexp->attribute.T.type];
    attribute.V.ConstTag = eqexp->attribute.V.ConstTag & relexp->attribute.V.ConstTag;
    if (!attribute.V.ConstTag) {
        return;
    }
    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (eqexp->attribute.T.type == relexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = eqexp->attribute.V.val.IntVal != relexp->attribute.V.val.IntVal;

        } else if (eqexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = int(eqexp->attribute.V.val.BoolVal) != relexp->attribute.V.val.IntVal;

        } else {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = eqexp->attribute.V.val.IntVal != int(relexp->attribute.V.val.BoolVal);
        }

    } else if (t == Type::FLOAT) {
        if (eqexp->attribute.T.type == relexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = eqexp->attribute.V.val.FloatVal != relexp->attribute.V.val.FloatVal;

        } else if (eqexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = float(eqexp->attribute.V.val.BoolVal) != relexp->attribute.V.val.FloatVal;

        } else if (eqexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.BoolVal = float(eqexp->attribute.V.val.IntVal) != relexp->attribute.V.val.FloatVal;

        } else if (relexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = eqexp->attribute.V.val.FloatVal != float(relexp->attribute.V.val.BoolVal);

        } else {
            // 另一个是 INT
            attribute.V.val.BoolVal = eqexp->attribute.V.val.FloatVal != float(relexp->attribute.V.val.IntVal);
        }

    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = int(eqexp->attribute.V.val.BoolVal) != int(relexp->attribute.V.val.BoolVal);
        // BOOL 类型的 != 运算
    }
}

void LAndExp_and::TypeCheck() {
    landexp->TypeCheck();
    eqexp->TypeCheck();

    // TODO("BinaryExp Semant");

    attribute.T.type = Type::BOOL;

    Type::ty t = GetCorrectType[landexp->attribute.T.type][eqexp->attribute.T.type];
    attribute.V.ConstTag = landexp->attribute.V.ConstTag & eqexp->attribute.V.ConstTag;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (landexp->attribute.T.type == eqexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = landexp->attribute.V.val.IntVal && eqexp->attribute.V.val.IntVal;

        } else if (landexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = int(landexp->attribute.V.val.BoolVal) && eqexp->attribute.V.val.IntVal;

        } else {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = landexp->attribute.V.val.IntVal && int(eqexp->attribute.V.val.BoolVal);
        }

    } else if (t == Type::FLOAT) {
        if (landexp->attribute.T.type == eqexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = landexp->attribute.V.val.FloatVal && eqexp->attribute.V.val.FloatVal;

        } else if (landexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = float(landexp->attribute.V.val.BoolVal) && eqexp->attribute.V.val.FloatVal;

        } else if (landexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.BoolVal = float(landexp->attribute.V.val.IntVal) && eqexp->attribute.V.val.FloatVal;

        } else if (eqexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = landexp->attribute.V.val.FloatVal && float(eqexp->attribute.V.val.BoolVal);

        } else {
            // 另一个是 INT
            attribute.V.val.BoolVal = landexp->attribute.V.val.FloatVal && float(eqexp->attribute.V.val.IntVal);
        }

    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = landexp->attribute.V.val.BoolVal && eqexp->attribute.V.val.BoolVal;
        // BOOL 类型的 && 运算
    }
}

void LOrExp_or::TypeCheck() {
    lorexp->TypeCheck();
    landexp->TypeCheck();

    // TODO("BinaryExp Semant");

    attribute.T.type = Type::BOOL;

    Type::ty t = GetCorrectType[lorexp->attribute.T.type][landexp->attribute.T.type];
    attribute.V.ConstTag = lorexp->attribute.V.ConstTag & landexp->attribute.V.ConstTag;
    if (!attribute.V.ConstTag) {
        return;
    }

    if (t == Type::VOID) {
        error_msgs.push_back("Invalid operands in line " + std::to_string(line_number) + "\n");
    } else if (t == Type::INT) {
        if (lorexp->attribute.T.type == landexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = lorexp->attribute.V.val.IntVal || landexp->attribute.V.val.IntVal;

        } else if (lorexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = int(lorexp->attribute.V.val.BoolVal) || landexp->attribute.V.val.IntVal;

        } else {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = lorexp->attribute.V.val.IntVal || int(landexp->attribute.V.val.BoolVal);
        }

    } else if (t == Type::FLOAT) {
        if (lorexp->attribute.T.type == landexp->attribute.T.type) {
            // 两个操作数类型相同
            attribute.V.val.BoolVal = lorexp->attribute.V.val.FloatVal || landexp->attribute.V.val.FloatVal;

        } else if (lorexp->attribute.T.type == Type::BOOL) {
            // 其中一个是 BOOL
            attribute.V.val.BoolVal = float(lorexp->attribute.V.val.BoolVal) || landexp->attribute.V.val.FloatVal;

        } else if (lorexp->attribute.T.type == Type::INT) {
            // 其中一个是 INT
            attribute.V.val.BoolVal = float(lorexp->attribute.V.val.IntVal) || landexp->attribute.V.val.FloatVal;

        } else if (landexp->attribute.T.type == Type::BOOL) {
            // 另一个是 BOOL
            attribute.V.val.BoolVal = lorexp->attribute.V.val.FloatVal || float(landexp->attribute.V.val.BoolVal);

        } else {
            // 另一个是 INT
            attribute.V.val.BoolVal = lorexp->attribute.V.val.FloatVal || float(landexp->attribute.V.val.IntVal);
        }

    } else if (t == Type::BOOL) {
        attribute.V.val.BoolVal = lorexp->attribute.V.val.BoolVal || landexp->attribute.V.val.BoolVal;
        // BOOL 类型的 || 运算
    }
}

void ConstExp::TypeCheck() {
    addexp->TypeCheck();
    attribute = addexp->attribute;
    if (!attribute.V.ConstTag) {    // addexp is not const
        error_msgs.push_back("Expression is not const " + std::to_string(line_number) + "\n");
    }
}

void Lval::TypeCheck() {
    // TODO("Lval Semant");

    VarAttribute var;
    if (semant_table.symbol_table.lookup_scope(name) != -1) {
        var = semant_table.symbol_table.lookup_val(name);
        scope = semant_table.symbol_table.lookup_scope(name);
    } else if (semant_table.GlobalTable.find(name) != semant_table.GlobalTable.end()) {
        var = semant_table.GlobalTable[name];
        scope = 0;
    } else {
        error_msgs.push_back("undeclared variable '" + name->get_string() + "' in line " + std::to_string(line_number) +
                             "\n");
        return;
    }
    std::vector<Expression> dim_vector;
    std::vector<int> lval_dims;
    if (dims) {
        dim_vector = *dims;
    }
    for (auto d : dim_vector) {
        d->TypeCheck();
        if (d->attribute.T.type == Type::FLOAT) {
            error_msgs.push_back("Array Dim can not be float in line " + std::to_string(line_number) + "\n");
        }
        if (d->attribute.T.type == Type::VOID) {
            error_msgs.push_back("Array Dim can not be void type in line " + std::to_string(line_number) + "\n");
        }
        lval_dims.push_back(d->attribute.V.val.IntVal);
    }

    if (var.dims.size() > lval_dims.size()) {
        // 数组
        attribute.T.type = Type::PTR;
        attribute.V.ConstTag = false;
    } else if (var.dims.size() == lval_dims.size()) {
        // 非数组
        attribute.T.type = var.type;
        attribute.V.ConstTag = var.ConstTag;
        if (var.ConstTag) {
            if (var.dims.size() == 0) {
                // 定义的值不是数组
                if (attribute.T.type == Type::INT) {
                    attribute.V.val.IntVal = var.IntInitVals[0];
                } else if (attribute.T.type == Type::FLOAT) {
                    attribute.V.val.FloatVal = var.FloatInitVals[0];
                }
            } else {
                // 定义的值是数组
                if (attribute.T.type == Type::INT) {
                    attribute.V.val.IntVal = getIntArrayVal(var, lval_dims);
                } else if (attribute.T.type == Type::FLOAT) {
                    attribute.V.val.FloatVal = getFloatArrayVal(var, lval_dims);
                }
            }
        }
    }
}

void FuncRParams::TypeCheck() { TODO("FuncRParams Semant"); }

void Func_call::TypeCheck() {
    // TODO("FunctionCall Semant");

    // 参数数量
    int len = 0;
    if (funcr_params) {
        auto params = dynamic_cast<FuncRParams *>(funcr_params)->params;
        len = params->size();
        for (auto parm : *params) {
            parm->TypeCheck();
            // std::cout << parm->attribute.GetAttributeInfo();
            if (parm->attribute.T.type == Type::VOID) {
                error_msgs.push_back("Funcr_params is void type in line " + std::to_string(line_number) + "\n");
                return;
            }
        }
    }

    if (semant_table.FunctionTable.find(name) == semant_table.FunctionTable.end()) {
        error_msgs.push_back("Call an undeclared function in line " + std::to_string(line_number) + "\n");
        return;
    } else {
        auto def = semant_table.FunctionTable[name];
        if (def->formals->size() != len) {
            error_msgs.push_back("Params do not match in line " + std::to_string(line_number) + "\n");
        }
        attribute.T.type = def->return_type;
        attribute.V.ConstTag = false;
    }
}

void UnaryExp_plus::TypeCheck() {
    // TODO("UnaryExp Semant");
    unary_exp->TypeCheck();
    if (unary_exp->attribute.T.type == Type::INT) {
        attribute.T.type = Type::INT;
        attribute.V.ConstTag = unary_exp->attribute.V.ConstTag;
        if (attribute.V.ConstTag) {
            attribute.V.val.IntVal = unary_exp->attribute.V.val.IntVal;
        }
    } else if (unary_exp->attribute.T.type == Type::FLOAT) {
        attribute.T.type = Type::FLOAT;
        attribute.V.ConstTag = unary_exp->attribute.V.ConstTag;
        if (attribute.V.ConstTag) {
            attribute.V.val.FloatVal = unary_exp->attribute.V.val.FloatVal;
        }
    } else {
        attribute.T.type = Type::INT;
        attribute.V.ConstTag = unary_exp->attribute.V.ConstTag;
        if (attribute.V.ConstTag) {
            attribute.V.val.IntVal = int(unary_exp->attribute.V.val.BoolVal);
        }
    }
}

void UnaryExp_neg::TypeCheck() {
    // TODO("UnaryExp Semant");
    unary_exp->TypeCheck();
    if (unary_exp->attribute.T.type == Type::INT) {
        attribute.T.type = Type::INT;
        attribute.V.ConstTag = unary_exp->attribute.V.ConstTag;
        if (attribute.V.ConstTag) {
            attribute.V.val.IntVal = -1 * unary_exp->attribute.V.val.IntVal;
        }
    } else if (unary_exp->attribute.T.type == Type::FLOAT) {
        attribute.T.type = Type::FLOAT;
        attribute.V.ConstTag = unary_exp->attribute.V.ConstTag;
        if (attribute.V.ConstTag) {
            attribute.V.val.FloatVal = -1 * unary_exp->attribute.V.val.FloatVal;
        }
    } else {
        attribute.T.type = Type::INT;
        attribute.V.ConstTag = unary_exp->attribute.V.ConstTag;
        if (attribute.V.ConstTag) {
            attribute.V.val.IntVal = -1 * int(unary_exp->attribute.V.val.BoolVal);
        }
    }
}

void UnaryExp_not::TypeCheck() {
    //  TODO("UnaryExp Semant");
    unary_exp->TypeCheck();

    attribute.T.type = Type::BOOL;
    attribute.V.ConstTag = unary_exp->attribute.V.ConstTag;

    if (unary_exp->attribute.T.type == Type::INT) {
        if (attribute.V.ConstTag) {
            attribute.V.val.IntVal = !unary_exp->attribute.V.val.IntVal;
        }
    } else if (unary_exp->attribute.T.type == Type::FLOAT) {
        if (attribute.V.ConstTag) {
            attribute.V.val.FloatVal = !unary_exp->attribute.V.val.FloatVal;
        }
    } else {
        if (attribute.V.ConstTag) {
            attribute.V.val.IntVal = !unary_exp->attribute.V.val.BoolVal;
        }
    }
}

void IntConst::TypeCheck() {
    attribute.T.type = Type::INT;
    attribute.V.ConstTag = true;
    attribute.V.val.IntVal = val;
}

void FloatConst::TypeCheck() {
    attribute.T.type = Type::FLOAT;
    attribute.V.ConstTag = true;
    attribute.V.val.FloatVal = val;
}

void StringConst::TypeCheck() { TODO("StringConst Semant"); }

void PrimaryExp_branch::TypeCheck() {
    exp->TypeCheck();
    attribute = exp->attribute;
}

void assign_stmt::TypeCheck() {
    // TODO("AssignStmt Semant");

    lval->TypeCheck();
    exp->TypeCheck();
    // zHere();
    if (lval->attribute.T.type == Type::VOID) {
        error_msgs.push_back("Void type cannot be assigned in line " + std::to_string(line_number) + "\n");
    }
    if (exp->attribute.T.type == Type::VOID) {
        error_msgs.push_back("Void type cannot be assign_stmt's right part in line " + std::to_string(line_number) +
                             "\n");
    }
}

void expr_stmt::TypeCheck() {
    exp->TypeCheck();
    attribute = exp->attribute;
}

void block_stmt::TypeCheck() { b->TypeCheck(); }

void ifelse_stmt::TypeCheck() {
    Cond->TypeCheck();
    if (Cond->attribute.T.type == Type::VOID) {
        error_msgs.push_back("if cond type is invalid " + std::to_string(line_number) + "\n");
    }
    ifstmt->TypeCheck();
    elsestmt->TypeCheck();
}

void if_stmt::TypeCheck() {
    Cond->TypeCheck();
    if (Cond->attribute.T.type == Type::VOID) {
        error_msgs.push_back("if cond type is invalid " + std::to_string(line_number) + "\n");
    }
    ifstmt->TypeCheck();
}

void while_stmt::TypeCheck() {
    // TODO("WhileStmt Semant");
    Cond->TypeCheck();
    if (Cond->attribute.T.type == Type::VOID) {
        error_msgs.push_back("Invalid while cond in line " + std::to_string(line_number) + "\n");
    }

    WhileCount += 1;
    body->TypeCheck();
    WhileCount -= 1;
}

void continue_stmt::TypeCheck() {
    // TODO("ContinueStmt Semant");

    // 不在循环语句中应该报错
    if (WhileCount == 0) {
        error_msgs.push_back("Continue is not in while_stmt in line " + std::to_string(line_number) + "\n");
    }
}

void break_stmt::TypeCheck() {
    // TODO("BreakStmt Semant");

    // 不在循环语句中应该报错
    if (WhileCount == 0) {
        error_msgs.push_back("Break is not in while_stmt in line " + std::to_string(line_number) + "\n");
    }
}

void return_stmt::TypeCheck() {
    return_exp->TypeCheck();
    if (return_exp->attribute.T.type == Type::VOID) {
        error_msgs.push_back("Void return type in line " + std::to_string(line_number) + "\n");
    }
}

void return_stmt_void::TypeCheck() {}

void ConstInitVal::TypeCheck() {
    // TODO("ConstInitVal Semant");

    auto vals = *initval;
    for (auto val : vals) {
        val->TypeCheck();
    }
}

void ConstInitVal_exp::TypeCheck() {
    // TODO("ConstInitVal_exp Semant");

    exp->TypeCheck();
    attribute = exp->attribute;

    if (attribute.T.type == Type::VOID) {
        error_msgs.push_back("ConstInitVal expression is void in line " + std::to_string(line_number) + "\n");
    }
    if (attribute.V.ConstTag == false) {
        error_msgs.push_back("ConstInitVal expression is not const in line " + std::to_string(line_number) + "\n");
    }
}

void VarInitVal::TypeCheck() {
    // TODO("VarInitVal Semant");

    auto vals = *initval;
    for (auto val : vals) {
        val->TypeCheck();    // 调用 VarInitVal_exp::TypeCheck()
    }
}

void VarInitVal_exp::TypeCheck() {
    // TODO("VarInitVal_exp Semant")

    exp->TypeCheck();
    attribute = exp->attribute;    // 将子表达式 exp 的属性赋值给当前节点

    if (attribute.T.type == Type::VOID) {
        error_msgs.push_back("VarInitVal expression is void in line" + std::to_string(line_number) + "\n");
    }
}

void VarDef_no_init::TypeCheck() { TODO("VarDefNoInit Semant"); }

void VarDef::TypeCheck() { TODO("VarDef Semant"); }

void ConstDef::TypeCheck() { TODO("ConstDef Semant"); }

void VarDecl::TypeCheck() {
    // 检查变量声明的类型是否为有效的类型（INT, FLOAT, BOOL）
    if (type_decl != Type::INT && type_decl != Type::FLOAT && type_decl != Type::BOOL) {
        // 如果变量类型无效，添加错误信息到错误信息列表中
        error_msgs.push_back("Invalid variable type declaration in line " + std::to_string(line_number) + "\n");
        return;    // 退出函数，不再进行类型检查
    }

    // 检查变量定义列表是否为空
    if (var_def_list == nullptr)
        return;    // 如果为空，直接退出函数

    // 获取变量定义列表的内容
    auto var_list_vector = *var_def_list;
    // 判断当前作用域是否为全局作用域（全局作用域的级别为0）
    bool is_global = (semant_table.symbol_table.get_current_scope() == 0);

    // 遍历变量定义列表中的每个变量
    for (auto var : var_list_vector) {
        // 如果是全局变量，检查该变量是否已经在全局符号表中声明
        if (is_global) {
            if (semant_table.GlobalTable.find(var->GetName()) != semant_table.GlobalTable.end()) {
                // 如果重复声明，添加错误信息到错误信息列表中
                error_msgs.push_back("Global variable '" + var->GetName()->get_string() +
                                     "' redeclaration error in line " + std::to_string(line_number) + "\n");
            }
        } else {    // 如果是局部变量，检查变量是否已经在当前作用域中声明
            if (semant_table.symbol_table.lookup_scope(var->GetName()) ==
                semant_table.symbol_table.get_current_scope()) {
                // 如果重复声明，添加错误信息到错误信息列表中
                error_msgs.push_back("Variable '" + var->GetName()->get_string() + "' redeclaration error in line " +
                                     std::to_string(line_number) + "\n");
            }
        }

        // 设置变量的作用域信息
        var->scope = is_global ? 0 : semant_table.symbol_table.get_current_scope();
        // 初始化存储维度信息的向量和计算数组大小的变量
        std::vector<int> dims;
        size_t size = 1;

        // 检查变量是否有维度信息（即是否为数组）
        if (var->GetDims() != nullptr) {
            // 获取维度信息列表
            auto dim_vector = *var->GetDims();
            // 遍历每个维度信息
            for (auto dim : dim_vector) {
                dim->TypeCheck();
                // 检查维度是否为常量表达式，如果不是，添加错误信息
                if (!dim->attribute.V.ConstTag) {
                    error_msgs.push_back("Array Dim must be const expression in line " + std::to_string(line_number) +
                                         "\n");
                }
                // 检查维度类型是否为浮点数，如果是，添加错误信息
                if (dim->attribute.T.type == Type::FLOAT) {
                    error_msgs.push_back("Array Dim can not be float in line " + std::to_string(line_number) + "\n");
                }
                // 获取维度的值，并将其存储到dims向量中
                int dim_val = dim->attribute.V.val.IntVal;
                dims.push_back(dim_val);
                // 计算数组的大小（所有维度的乘积）
                size *= dim_val;
            }
        }

        // 初始化变量属性信息
        VarAttribute var_attr;
        var_attr.ConstTag = false;
        var_attr.type = type_decl;
        var_attr.dims = dims;

        // 检查变量是否有初始值
        if (var->GetInitVal() != nullptr) {
            // 对初始值进行类型检查
            var->GetInitVal()->TypeCheck();

            // 根据变量类型处理初始值
            if (type_decl == Type::INT) {
                // 如果变量为整数类型且初始值是一个数组
                if (var->GetInitVal()->GetInitValArray()) {
                    std::vector<int> result(size, 0);
                    // 填充整数数组，index为填充后的位置，如果超出数组长度，添加错误信息
                    size_t index = fillIntArray(var->GetInitVal()->GetInitValArray(), result, dims, 0, 0, false);
                    if (index > size) {
                        error_msgs.push_back("Initial values exceed array dimensions in line " +
                                             std::to_string(line_number) + "\n");
                    }
                    var_attr.IntInitVals = result;    // 将填充后的数组设置为变量的初始值
                } else {                              // 如果变量为整数类型且初始值不是一个数组
                    // 根据初始值的类型进行相应的处理和转换
                    switch (var->GetInitVal()->attribute.T.type) {
                    case Type::INT:
                        var_attr.IntInitVals.push_back(var->GetInitVal()->attribute.V.val.IntVal);
                        break;    // 直接将整数值添加到初始值列表中
                    case Type::FLOAT:
                        var_attr.IntInitVals.push_back(int(var->GetInitVal()->attribute.V.val.FloatVal));
                        break;    // 将浮点数值转换为整数后添加到初始值列表中
                    case Type::BOOL:
                        var_attr.IntInitVals.push_back(int(var->GetInitVal()->attribute.V.val.BoolVal));
                        break;    // 将布尔值转换为整数后添加到初始值列表中
                    }
                }
            } else if (type_decl == Type::FLOAT) {    // 如果变量为浮点数类型
                // 如果变量为浮点数类型且初始值是一个数组
                if (var->GetInitVal()->GetInitValArray()) {
                    // 初始化一个浮点数类型的数组，长度为size，初始值为0
                    std::vector<float> result(size, 0);
                    // 填充浮点数数组，index为填充后的位置，如果超出数组长度，添加错误信息
                    size_t index = fillFloatArray(var->GetInitVal()->GetInitValArray(), result, dims, 0, 0, false);
                    if (index > size) {
                        error_msgs.push_back("Initial values exceed array dimensions in line " +
                                             std::to_string(line_number) + "\n");
                    }
                    var_attr.FloatInitVals = result;    // 将填充后的数组设置为变量的初始值
                } else {                                // 如果变量为浮点数类型且初始值不是一个数组
                    // 根据初始值的类型进行相应的处理和转换
                    switch (var->GetInitVal()->attribute.T.type) {
                    case Type::INT:
                        var_attr.FloatInitVals.push_back(float(var->GetInitVal()->attribute.V.val.IntVal));
                        break;    // 将整数值转换为浮点数后添加到初始值列表中
                    case Type::FLOAT:
                        var_attr.FloatInitVals.push_back(var->GetInitVal()->attribute.V.val.FloatVal);
                        break;    // 直接将浮点数值添加到初始值列表中
                    case Type::BOOL:
                        var_attr.FloatInitVals.push_back(float(int(var->GetInitVal()->attribute.V.val.BoolVal)));
                        break;    // 将布尔值先转换为整数，再转换为浮点数后添加到初始值列表中
                    }
                }
            }
        } else {    // 如果变量没有初始值，根据变量类型设置默认初始值
            if (type_decl == Type::INT) {
                var_attr.IntInitVals.resize(size, 0);    // 默认初始值0
            } else if (type_decl == Type::FLOAT) {
                var_attr.FloatInitVals.resize(size, 0);
            }
        }

        // 将变量属性信息添加到相应的符号表中
        if (is_global) {    // 如果是全局变量
            semant_table.GlobalVarTable[var->GetName()] = var_attr;
            semant_table.GlobalTable[var->GetName()] = var_attr;
        } else {    // 如果是局部变量
            semant_table.symbol_table.add_Symbol(var->GetName(), var_attr);
        }
    }
}

void ConstDecl::TypeCheck() {
    if (type_decl == Type::INT || type_decl == Type::FLOAT || type_decl == Type::BOOL) {
        if (var_def_list != nullptr) {
            auto var_list_vector = *var_def_list;
            if (semant_table.symbol_table.get_current_scope() == 0) {
                // 全局变量声明
                for (auto var : var_list_vector) {
                    if (semant_table.GlobalTable.find(var->GetName()) != semant_table.GlobalTable.end()) {
                        error_msgs.push_back("Global variable '" + var->GetName()->get_string() +
                                             "' redeclaration error in line " + std::to_string(line_number) + "\n");
                    }
                    var->scope = 0;
                    std::vector<int> dims;
                    if (var->GetDims() != nullptr) {
                        auto dim_vector = *var->GetDims();
                        for (auto dim : dim_vector) {
                            dim->TypeCheck();
                            if (dim->attribute.V.ConstTag == false) {
                                error_msgs.push_back("Array Dim must be const expression in line " +
                                                     std::to_string(line_number) + "\n");
                            }
                            if (dim->attribute.T.type == Type::FLOAT) {
                                error_msgs.push_back("Array Dim can not be float in line " +
                                                     std::to_string(line_number) + "\n");
                            }
                            dims.push_back(dim->attribute.V.val.IntVal);
                        }
                    }

                    VarAttribute var_attr;
                    var_attr.ConstTag = true;
                    var_attr.type = type_decl;
                    var_attr.dims = dims;

                    if (var->GetInitVal() != nullptr) {
                        var->GetInitVal()->TypeCheck();

                        size_t size = 1;
                        for (auto dim : dims) {
                            size *= dim;
                        }

                        if (type_decl == Type::INT) {
                            if (var->GetInitVal()->GetInitValArray()) {
                                // InitVal -> {InitVal, InitVal...}
                                std::vector<int> result(size, 0);

                                size_t index =
                                fillIntArray(var->GetInitVal()->GetInitValArray(), result, dims, 0, 0, true);
                                if (index > size) {
                                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                                         std::to_string(line_number) + "\n");
                                }
                                var_attr.IntInitVals = result;
                            } else {
                                // InitVal -> InitVal
                                if (var->GetInitVal()->attribute.T.type == Type::INT) {
                                    var_attr.IntInitVals.push_back(var->GetInitVal()->attribute.V.val.IntVal);
                                } else if (var->GetInitVal()->attribute.T.type == Type::FLOAT) {
                                    var_attr.IntInitVals.push_back(int(var->GetInitVal()->attribute.V.val.FloatVal));
                                } else if (var->GetInitVal()->attribute.T.type == Type::BOOL) {
                                    var_attr.IntInitVals.push_back(int(var->GetInitVal()->attribute.V.val.BoolVal));
                                }
                            }

                        } else if (type_decl == Type::FLOAT) {
                            if (var->GetInitVal()->GetInitValArray()) {
                                // InitVal -> {InitVal, InitVal...}
                                std::vector<float> result(size, 0);
                                size_t index =
                                fillFloatArray(var->GetInitVal()->GetInitValArray(), result, dims, 0, 0, true);
                                if (index > size) {
                                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                                         std::to_string(line_number) + "\n");
                                }
                                var_attr.FloatInitVals = result;
                            } else {
                                // InitVal -> InitVal
                                if (var->GetInitVal()->attribute.T.type == Type::INT) {
                                    var_attr.FloatInitVals.push_back(float(var->GetInitVal()->attribute.V.val.IntVal));
                                } else if (var->GetInitVal()->attribute.T.type == Type::FLOAT) {
                                    var_attr.FloatInitVals.push_back(var->GetInitVal()->attribute.V.val.FloatVal);
                                } else {
                                    var_attr.FloatInitVals.push_back(
                                    float(int(var->GetInitVal()->attribute.V.val.BoolVal)));
                                }
                            }
                        }
                    }
                    semant_table.GlobalTable[var->GetName()] = var_attr;
                    semant_table.GlobalVarTable[var->GetName()] = var_attr;
                }
            } else {
                for (auto var : var_list_vector) {
                    // 检查同一作用域下重复声明的变量
                    if (semant_table.symbol_table.lookup_scope(var->GetName()) ==
                        semant_table.symbol_table.get_current_scope()) {
                        error_msgs.push_back("Constant '" + var->GetName()->get_string() +
                                             "' redeclaration error in line " + std::to_string(line_number) + "\n");
                    }

                    // 作用域
                    var->scope = semant_table.symbol_table.get_current_scope();

                    // 检查维度信息
                    std::vector<int> dims;
                    if (var->GetDims() != nullptr) {
                        auto dim_vector = *var->GetDims();
                        for (auto dim : dim_vector) {
                            dim->TypeCheck();
                            if (dim->attribute.V.ConstTag == false) {
                                error_msgs.push_back("Array Dim must be const expression in line " +
                                                     std::to_string(line_number) + "\n");
                            }
                            if (dim->attribute.T.type == Type::FLOAT) {
                                error_msgs.push_back("Array Dim can not be float in line " +
                                                     std::to_string(line_number) + "\n");
                            }
                            dims.push_back(dim->attribute.V.val.IntVal);
                        }
                    }

                    VarAttribute var_attr;
                    var_attr.ConstTag = true;
                    var_attr.type = type_decl;
                    var_attr.dims = dims;

                    if (var->GetInitVal() != nullptr) {
                        var->GetInitVal()->TypeCheck();

                        size_t size = 1;
                        for (auto dim : dims) {
                            size *= dim;
                        }

                        if (type_decl == Type::INT) {
                            if (var->GetInitVal()->GetInitValArray()) {
                                // InitVal -> {InitVal, InitVal...}
                                std::vector<int> result(size, 0);

                                size_t index =
                                fillIntArray(var->GetInitVal()->GetInitValArray(), result, dims, 0, 0, true);
                                if (index > size) {
                                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                                         std::to_string(line_number) + "\n");
                                }
                                var_attr.IntInitVals = result;
                            } else {
                                // InitVal -> InitVal
                                if (var->GetInitVal()->attribute.T.type == Type::INT) {
                                    var_attr.IntInitVals.push_back(var->GetInitVal()->attribute.V.val.IntVal);
                                } else if (var->GetInitVal()->attribute.T.type == Type::FLOAT) {
                                    var_attr.IntInitVals.push_back(int(var->GetInitVal()->attribute.V.val.FloatVal));
                                } else if (var->GetInitVal()->attribute.T.type == Type::BOOL) {
                                    var_attr.IntInitVals.push_back(int(var->GetInitVal()->attribute.V.val.BoolVal));
                                }
                            }

                        } else if (type_decl == Type::FLOAT) {
                            if (var->GetInitVal()->GetInitValArray()) {
                                // InitVal -> {InitVal, InitVal...}
                                std::vector<float> result(size, 0);
                                size_t index =
                                fillFloatArray(var->GetInitVal()->GetInitValArray(), result, dims, 0, 0, true);
                                if (index > size) {
                                    error_msgs.push_back("Initial values exceed array dimensions in line " +
                                                         std::to_string(line_number) + "\n");
                                }
                                var_attr.FloatInitVals = result;
                            } else {
                                // InitVal -> InitVal
                                if (var->GetInitVal()->attribute.T.type == Type::INT) {
                                    var_attr.FloatInitVals.push_back(float(var->GetInitVal()->attribute.V.val.IntVal));
                                } else if (var->GetInitVal()->attribute.T.type == Type::FLOAT) {
                                    var_attr.FloatInitVals.push_back(var->GetInitVal()->attribute.V.val.FloatVal);
                                } else {
                                    var_attr.FloatInitVals.push_back(
                                    float(int(var->GetInitVal()->attribute.V.val.BoolVal)));
                                }
                            }
                        }
                    }

                    // Add the constant declaration to the symbol table
                    semant_table.symbol_table.add_Symbol(var->GetName(), var_attr);
                }
            }
        }
    } else {
        error_msgs.push_back("Invalid constant type declaration in line " + std::to_string(line_number) + "\n");
    }
}

void BlockItem_Decl::TypeCheck() { decl->TypeCheck(); }

void BlockItem_Stmt::TypeCheck() { stmt->TypeCheck(); }

void __Block::TypeCheck() {
    semant_table.symbol_table.enter_scope();
    auto item_vector = *item_list;
    for (auto item : item_vector) {
        item->TypeCheck();
    }
    semant_table.symbol_table.exit_scope();
}

void __FuncFParam::TypeCheck() {
    VarAttribute val;
    val.ConstTag = false;
    val.type = type_decl;
    scope = 1;

    // 如果dims为nullptr, 表示该变量不含数组下标, 如果你在语法分析中采用了其他方式处理，这里也需要更改
    if (dims != nullptr) {
        auto dim_vector = *dims;

        // the fisrt dim of FuncFParam is empty
        // eg. int f(int A[][30][40])
        val.dims.push_back(-1);    // 这里用-1表示empty，你也可以使用其他方式
        for (int i = 1; i < dim_vector.size(); ++i) {
            auto d = dim_vector[i];
            d->TypeCheck();
            if (d->attribute.V.ConstTag == false) {
                error_msgs.push_back("Array Dim must be const expression in line " + std::to_string(line_number) +
                                     "\n");
            }
            if (d->attribute.T.type == Type::FLOAT) {
                error_msgs.push_back("Array Dim can not be float in line " + std::to_string(line_number) + "\n");
            }
            val.dims.push_back(d->attribute.V.val.IntVal);
        }
        attribute.T.type = Type::PTR;
    } else {
        attribute.T.type = type_decl;
    }

    if (name != nullptr) {
        if (semant_table.symbol_table.lookup_scope(name) != -1) {
            error_msgs.push_back("multiple difinitions of formals in function " + name->get_string() + " in line " +
                                 std::to_string(line_number) + "\n");
        }
        semant_table.symbol_table.add_Symbol(name, val);
    }
}

void __FuncDef::TypeCheck() {
    semant_table.symbol_table.enter_scope();

    if (semant_table.FunctionTable.find(name) != semant_table.FunctionTable.end()) {
        error_msgs.push_back("Function '" + name->get_string() + "' redeclaration in line " +
                             std::to_string(line_number) + "\n");
    }
    // 判断是否有 main 函数
    if (name->get_string() == "main") {
        haveMain = true;
    }
    semant_table.FunctionTable[name] = this;

    auto formal_vector = *formals;
    for (auto formal : formal_vector) {
        formal->TypeCheck();
    }

    // block TypeCheck
    if (block != nullptr) {
        auto item_vector = *(block->item_list);
        for (auto item : item_vector) {
            item->TypeCheck();
        }
    }

    semant_table.symbol_table.exit_scope();
}

void CompUnit_Decl::TypeCheck() {
    // TODO("CompUnitDecl Semant");

    decl->TypeCheck();
    // maybe do something
}

void CompUnit_FuncDef::TypeCheck() { func_def->TypeCheck(); }