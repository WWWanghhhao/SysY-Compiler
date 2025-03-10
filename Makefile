SRCDIR += ./target
SRCDIR += ./target/common
SRCDIR += ./target/common/machine_passes/register_alloc
SRCDIR += ./target/common/machine_passes/register_alloc/fast_linear_scan
SRCDIR += ./target/common/machine_instruction_structures
SRCDIR += ./target/common/machine_instruction_structures/cfg_iterators
SRCDIR += ./target/common/machine_passes
SRCDIR += ./target/riscv64gc
SRCDIR += ./target/riscv64gc/instruction_print
SRCDIR += ./target/riscv64gc/instruction_select
SRCDIR += ./target/riscv64gc/optimize
SRCDIR += ./parser
SRCDIR += ./lexer
SRCDIR += ./ir_gen
SRCDIR += ./include
SRCDIR += ./optimize/analysis
SRCDIR += ./optimize/transform
SRCDIR += ./optimize
SRCDIR += ./utils

NAME = SysYc
BIN_DIR ?= ./bin
OBJDIR ?= ./obj
BINARY ?= $(BIN_DIR)/$(NAME)

.DEFAULT_GOAL = SysYc

CC = clang++
LD = clang++
INCLUDES = $(addprefix -I, $(SRCDIR))
CFLAGS += -O2 -g -MMD -std=c++20 $(INCLUDES)

# SRCS = $(shell find . -name "*.cc")
# OBJS = $(SRCS:%.cc=$(OBJDIR)/%.o)
SRCS := $(foreach dir,$(SRCDIR),$(wildcard $(dir)/*.cc))
OBJS := $(patsubst %.cc,$(OBJDIR)/%.o,$(SRCS))

$(OBJDIR)/%.o : %.cc
	@echo + CC $<
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS)  -c -o $@ $<

-include $(OBJS:.o=.d)

.PHONY : SysYc clean-obj clean-all lexer parser format

SysYc : $(BINARY)

$(BINARY): $(OBJS)
	@echo + LD $@
	@$(LD) $(OBJS) -o bin/SysYc -O2 -std=c++17

CASE ?= dummy
STAGE ?= S
OFLAG ?= O1
ARCH ?= rv64gc
# OPTION ?= -p

lexer:lexer/SysY_lexer.l
	flex -o lexer/SysY_lexer.cc lexer/SysY_lexer.l

parser:parser/SysY_parser.y
	bison -dv parser/SysY_parser.y
	rm -r SysY_parser.output
	mv SysY_parser.tab.c parser/SysY_parser.tab.cc
	mv SysY_parser.tab.h parser/SysY_parser.tab.h
	sed -i '1s/^/#include \"SysY_tree.h\"/' parser/SysY_parser.tab.h

clean-obj:
	rm -r ${OBJDIR}/*

clean-all:
	rm -r ${OBJDIR}/*
	rm -r bin/*

format:
	clang-format -style=file -i ${SRCS}
	clang-format -style=file -i $(foreach dir,$(SRCDIR),$(wildcard $(dir)/*.h))

# Input and output files
SRC2 = example.sy
ASM2 = example.out.s
OBJ2 = tmp.o
EXE2 = a.out

# Compiler and linker settings
CCC = riscv64-unknown-linux-gnu-gcc
CFLAGS2 = -static

# Compile the SysY source code to assembly
$(ASM2): $(SRC2)
	./bin/SysYc -S -o $(ASM2) $(SRC2) -O1

# Assemble the assembly code to object file
$(OBJ2): $(ASM2)
	$(CCC) $(CFLAGS2) -c $(ASM2) -o $(OBJ2)

# Link the object file with the library
$(EXE2): $(OBJ2) lib/libsysy_rv.a
	$(CCC) $(CFLAGS2) $(OBJ2) lib/libsysy_rv.a -o $(EXE2)

# Run the executable and print the return value
test: $(EXE2)
	qemu-riscv64 ./$(EXE2)
	echo $$?

# Clean up the generated files
clean:
	rm -f $(ASM2) $(OBJ2) $(EXE2)

# Default target
.PHONY: all
all: test