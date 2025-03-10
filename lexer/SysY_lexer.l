%option noyywrap
%{
#include <cmath>
#include <vector>
#include <cstring>
#include "../include/symtab.h"
#include "../parser/SysY_parser.tab.h"

extern int line_number;
extern int col_number;
extern int cur_col_number;
extern IdTable id_table;
void yyerror(char* s, ...);
%}


%x ANNOTATION

%%
    /* TODO():增加处理注释的代码*/

"//".* { //单行注释
    cur_col_number = col_number;
    col_number += strlen(yytext); 
}

"/*" { //多行注释
    cur_col_number = col_number;
    col_number += strlen(yytext);
    BEGIN(ANNOTATION);
}

"*/" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    yylval.error_msg = "only */ without /*";
    return ERROR;
}

<ANNOTATION>"*/" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    BEGIN(INITIAL);
}

<ANNOTATION>"\n" { 
    ++line_number; 
    col_number = 0; 
}

<ANNOTATION>. {
    cur_col_number = col_number;
    col_number += strlen(yytext);
}

    /* TODO():增加处理列号的代码(cur_col_number表示当前token开始位置, col_number表示当前token结束位置) */
"<=" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    return LEQ;
}         
">=" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    return GEQ;
}         
"==" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    return EQ;
}        
"!=" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    return NE;
}
"&&" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    return AND;
}         
"||" {
    cur_col_number = col_number;
    col_number += strlen(yytext);    
    return OR;
}               
"const" {
    cur_col_number = col_number;
    col_number += strlen(yytext); 
    return CONST;
}     
"if" {
    cur_col_number = col_number;
    col_number += strlen(yytext);    
    return IF;
}       
"else" {
    cur_col_number = col_number;
    col_number += strlen(yytext);  
    return ELSE;
}  
"while" {
    cur_col_number = col_number;
    col_number += strlen(yytext);    
    return WHILE;
}
"void" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    return NONE_TYPE;
}    
"int" {
    cur_col_number = col_number;
    col_number += strlen(yytext);    
    return INT;
}    
"float" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    return FLOAT;
}      
"return" {
    cur_col_number = col_number;
    col_number += strlen(yytext); 
    return RETURN;
}    
"break" {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    return BREAK;
}     
"continue" {
    cur_col_number = col_number;
    col_number += strlen(yytext);    
    return CONTINUE;
}

    /*添加其它终结符*/
"for" {
    cur_col_number = col_number;
    col_number += strlen(yytext);    
    return FOR;
}
"then" {
    cur_col_number = col_number;
    col_number += strlen(yytext);    
    return THEN;
}

\n {++line_number;col_number = 0;}

[ \t\f\r\v] {col_number += strlen(yytext);}

[\.\+\-\*\/\=\<\!\%\>] {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    return yytext[0];
}

[\{\}\;\(\)\,\[\]] {
    cur_col_number = col_number;
    col_number += strlen(yytext);    
    return yytext[0];
}

[_a-zA-Z][_a-zA-Z0-9]* {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    yylval.symbol_token = id_table.add_id(std::string(yytext));
    return IDENT;
}
([1-9][0-9]*)|0 {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    // yylval.int_token = stoi(std::string(yytext));
    yylval.int_token = 0;
    for(int i = 0;yytext[i];i++){
        yylval.int_token *= 10;
        yylval.int_token += yytext[i] - '0';
    }
    return INT_CONST;
}
    /*
      在词法分析中，你只需要考虑yylval的三种类型，分别为int_token, float_token, symbol_token
      之所以要定义这三种类型，是因为我们只返回这个token的类型是不够的，我们还需要知道这个token的属性
      例如我们需要知道INT_CONST具体的值是多少
    */
    /*TODO():参考SysY2022定义, 处理更多的词法, 如浮点数，十六进制数等*/



"0"[bB][01]+ {
    // 二进制整数常量
    cur_col_number = col_number;
    col_number += strlen(yytext);
    yylval.int_token = 0;
    int i = 0;
    // 跳过 0[bB] 两个字符
    i += 2;
    for(; yytext[i]; i++) {
        yylval.int_token *= 2;
        yylval.int_token += yytext[i] - '0';
    }    
    return INT_CONST;
}

"0"[0-7]* {
    // 八进制整数常量
    cur_col_number = col_number;
    col_number += strlen(yytext);
    yylval.int_token = 0;
    int i = 0;
    // 跳过 0 一个字符
    i += 1;
    for(; yytext[i]; i++) {
        yylval.int_token *= 8;
        yylval.int_token += yytext[i] - '0';
    }
    return INT_CONST;
}

"0"[xX][0-9a-fA-F]+ {
    // 十六进制整数常量
    cur_col_number = col_number;
    col_number += strlen(yytext);
    yylval.int_token = 0;
    int i = 0;
    // 跳过 0[xX] 两个字符
    i += 2;
    for(; yytext[i]; i++) {
        yylval.int_token = yylval.int_token * 16;
        if ('0' <= yytext[i] && yytext[i] <= '9') {
            yylval.int_token += yytext[i] - '0';
        } else if ('a' <= yytext[i] && yytext[i] <= 'f') {
            yylval.int_token += yytext[i] - 'a' + 10;
        } else if ('A' <= yytext[i] && yytext[i] <= 'F') {
            yylval.int_token += yytext[i] - 'A' + 10;
        }
    }
    return INT_CONST;
}

[0-9]*(\.[0-9]*)?([eE][\+\-]?[0-9]+)? {
    // 科学计数法
    cur_col_number = col_number;
    col_number += strlen(yytext);
    //yylval.float_token = std::stof(yytext);
    double result = 0.0;
    double exp = 0.0;
    int exp_sign = 1;
    int is_exp = 0;

    int i = 0;

    // 处理整数部分
    while (yytext[i] >= '0' && yytext[i] <= '9') {
        result = result * 10.0 + (yytext[i] - '0');
        i++;
    }

    // 处理小数部分
    if (yytext[i] == '.') {
        i++;
        double dec_place = 0.1;
        while (yytext[i] >= '0' && yytext[i] <= '9') {
            result += dec_place * (yytext[i] - '0');
            dec_place *= 0.1;
            i++;
        }
    }

    // 处理指数部分
    if (yytext[i] == 'e' || yytext[i] == 'E') {
        is_exp = 1;
        i++;
        if (yytext[i] == '-') {
            exp_sign = -1;
            i++;
        } else if (yytext[i] == '+') {
            i++;
        }
        while (yytext[i] >= '0' && yytext[i] <= '9') {
            exp = exp * 10.0 + (yytext[i] - '0');
            i++;
        }
    }

    // 计算最终结果
    if (is_exp) {
        exp *= exp_sign;
        result *= std::pow(10.0, exp);
    }
    yylval.float_token = result;
    return FLOAT_CONST;
}

"0"[xX][0-9a-fA-F]*\.[0-9a-fA-F]+[pP][\+\-]?[0-9]* {
    // 十六进制浮点常量
    cur_col_number = col_number;
    col_number += strlen(yytext);

    int i = 0;
    // 跳过 0[xX] 
    i += 2;

    double val = 0;
    int exp = 0;
    // 计算整数部分和小数部分，最后再除以小数位个 16 即可
    int frac_size = 0;
    while (i < strlen(yytext) && yytext[i] != 'p' && yytext[i] != 'P') {
        if (yytext[i] == '.') {
            frac_size++;
            i++;
            continue;
        }
        if (frac_size > 0) {
            frac_size++;
        }
        val = val * 16;
        if ('0' <= yytext[i] && yytext[i] <= '9') {
            val += yytext[i] - '0';
        } else if ('a' <= yytext[i] && yytext[i] <= 'f') {
            val += yytext[i] - 'a' + 10;
        } else if ('A' <= yytext[i] && yytext[i] <= 'F') {
            val += yytext[i] - 'A' + 10;
        }
        i++;
    }
    frac_size--;
    for (int j = 0;j < frac_size; j++) {
        val /= 16;
    }
    
    // 计算指数部分
    if (i < strlen(yytext) && (yytext[i] == 'p' || yytext[i] == 'P')) {
        i++;
        if (i < strlen(yytext)) {
            int exp_sign = 1;
            if (yytext[i] == '-') {
                exp_sign = -1;
                i++;
            } else if (yytext[i] == '+') {
                i++;
            }
            if (i < strlen(yytext)) {
                // 说明存在指数部分
                while (i < strlen(yytext)) {
                    exp = exp * 10 + (yytext[i] - '0');
                    i++;
                }
                exp *= exp_sign;
                val *= std::pow(2, exp);
                yylval.float_token = val;
                return FLOAT_CONST;
            }

        }
    }
    yylval.error_msg = yytext;
    return ERROR;

}

    /*unknown tokens, return ERROR*/
. {
    cur_col_number = col_number;
    col_number += strlen(yytext);
    yylval.error_msg = yytext;
    return ERROR;
}
%%



