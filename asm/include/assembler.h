#ifndef K_ASSEMBLER_H
#define K_ASSEMBLER_H

#include "file.h"
#include "spu_common.h"

#include <cstddef>
#include <cstdio>

constexpr int UNINITIALIZED_LABEL_VALUE = -1;
constexpr int DEFAULT_ARGUMENT_VALUE = -1337;

struct Label
{
    unsigned long hash = 0;
    ssize_t addressValue = 0;
    char *name = nullptr;
};

struct Assembler
{
    text_t sourceText = {};

    int *bytecode = nullptr;

    size_t instructionPointer = 0;
    size_t instructionsCapacity = 0;

    char *sourceFileName = nullptr;
    size_t currentLineNumber = 0; 

    Label labels[SPU_MAX_LABELS_COUNT] = {};
    size_t labelsCount = 0;
    
    FILE *listingFile = nullptr;
};

enum AssemblerError : int
{
    ASSEMBLER_OK                             = 0,
    ASSEMBLER_UNKNOWN_COMMAND                = 1 << 0,
    ASSEMBLER_MISSING_ARGUMENT               = 1 << 1,
    ASSEMBLER_WRONG_ARGUMENT_TYPE            = 1 << 2,
    ASSEMBLER_TRASH_SYMBOLS                  = 1 << 3,
    ASSEMBLER_INVALID_REGISTER_NAME          = 1 << 4,
    ASSEMBLER_INVALID_REGISTER_ADDRESS_NAME  = 1 << 5,
    ASSEMBLER_DUPLICATE_LABEL                = 1 << 6,
    ASSEMBLER_INVALID_LABEL                  = 1 << 7,
    ASSEMBLER_TRASH_AFTER_COMMAND            = 1 << 8,

    ASSEMBLER_COMMON_ERROR                   = 1 << 31
};

struct Command 
{
    const char *name = nullptr;
    int bytecode = 0;
    int allowedArgumentTypes = 0;
    size_t nameLength = 0;
};

enum ArgumentType : int
{
    ARG_TYPE_NONE               = 0,
    ARG_TYPE_NUMBER             = 1 << 0,
    ARG_TYPE_LABEL              = 1 << 1,
    ARG_TYPE_REGISTER           = 1 << 2,
    ARG_TYPE_MEMORY_ADDRESS     = 1 << 3,

    ARG_TYPE_UNKNOWN            = 1 << 31
};

enum AssemblyPass : int
{
    FIRST_PASS = 1,
    FINAL_PASS = 2
};

#define DEFINE_COMMAND(cmdName, argTypeEnum) {.name = #cmdName,           \
                                              .bytecode = SPU_##cmdName,  \
                                              .allowedArgumentTypes = argTypeEnum, \
                                              .nameLength = sizeof(#cmdName)}

constexpr Command COMMANDS_TABLE[] = 
{
    DEFINE_COMMAND(PUSH,  ARG_TYPE_NUMBER           ),
    DEFINE_COMMAND(POP,   ARG_TYPE_NONE             ),
    DEFINE_COMMAND(ADD,   ARG_TYPE_NONE             ),
    DEFINE_COMMAND(SUB,   ARG_TYPE_NONE             ),
    DEFINE_COMMAND(DIV,   ARG_TYPE_NONE             ),
    DEFINE_COMMAND(MUL,   ARG_TYPE_NONE             ),
    DEFINE_COMMAND(SQRT,  ARG_TYPE_NONE             ),
    DEFINE_COMMAND(OUT,   ARG_TYPE_NONE             ),
    DEFINE_COMMAND(IN,    ARG_TYPE_NONE             ),
    DEFINE_COMMAND(JMP,   ARG_TYPE_LABEL            ),
    DEFINE_COMMAND(JB,    ARG_TYPE_LABEL            ),
    DEFINE_COMMAND(JBE,   ARG_TYPE_LABEL            ),
    DEFINE_COMMAND(JA,    ARG_TYPE_LABEL            ),
    DEFINE_COMMAND(JAE,   ARG_TYPE_LABEL            ),
    DEFINE_COMMAND(JE,    ARG_TYPE_LABEL            ),
    DEFINE_COMMAND(JNE,   ARG_TYPE_LABEL            ),
    DEFINE_COMMAND(CALL,  ARG_TYPE_LABEL            ),
    DEFINE_COMMAND(RET,   ARG_TYPE_NONE             ),
    DEFINE_COMMAND(PUSHM, ARG_TYPE_MEMORY_ADDRESS   ),
    DEFINE_COMMAND(POPM,  ARG_TYPE_MEMORY_ADDRESS   ),
    DEFINE_COMMAND(PUSHR, ARG_TYPE_REGISTER         ),
    DEFINE_COMMAND(POPR,  ARG_TYPE_REGISTER         ),
    DEFINE_COMMAND(DRAW,  ARG_TYPE_NONE             ),
    DEFINE_COMMAND(HLT,   ARG_TYPE_NONE             )
};

constexpr size_t COMMANDS_COUNT = sizeof(COMMANDS_TABLE) / sizeof(Command);

#undef DEFINE_COMMAND

struct Argument
{
    ArgumentType type = ARG_TYPE_UNKNOWN;
    int value = DEFAULT_ARGUMENT_VALUE;
};

int AllocateBytecode(Assembler *assembler);
int InitializeAssembler(Assembler *assembler);
int ReadSourceFile(Assembler *assembler, char *inputFileName);
int AssembleProgram(Assembler *assembler);
int WriteBinaryOutput(const char *outputFileName, Assembler *assembler);
int PrintAssemblerError(int error);
int DestroyAssembler(Assembler *assembler);

#endif