#include <cassert>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>

#include "assembler.hpp"
#include "spu_common.hpp"
#include "file.hpp"
#include "debug.hpp"
#include "utils.hpp"

static int  GrowBytecodeIfNeeded    (Assembler *assembler);

static int  RunFirstPass            (Assembler *assembler);
static int  RunFinalPass            (Assembler *assembler);

static const Command *LookupCommand (char **linePtr);

static int  ProcessLine             (Assembler *assembler, size_t lineIdx, AssemblyPass pass);
static int  EmitCommand             (Assembler *assembler, const Command *command, char **linePtr);
static int  ParseArgument           (Assembler *assembler, Argument *argument, char **linePtr);

static int  RegisterLabel           (Assembler *assembler, char **linePtr);
static int  LookupLabel             (Assembler *assembler, const char *name, unsigned long hash);

static int  DecodeRegister          (char *name, int *bytesConsumed, int *bytecode);
static int  DecodeMemoryRegister    (char *name, int *bytesConsumed, int *bytecode);

static int  WriteFileHeader         (FILE *outputFile, Assembler *assembler);
static int  WriteFileBody           (FILE *outputFile, Assembler *assembler);

static int  EmitListingLine         (Assembler *assembler, size_t instructionStart);
static int  CheckForTrailingGarbage (Assembler *assembler, char *linePtr);


int InitializeAssembler (Assembler *assembler)
{
    assert (assembler);

    *assembler = {};

    assembler->listingFile = fopen ("listing.txt", "w");
    if (assembler->listingFile == nullptr)
    {
        perror ("Error opening listing file");
        return ASSEMBLER_COMMON_ERROR;
    }

    for (size_t i = 0; i < SPU_MAX_LABELS_COUNT; i++)
    {
        assembler->labels[i].addressValue = UNINITIALIZED_LABEL_VALUE;
        assembler->labels[i].hash         = 0;
    }

    return ASSEMBLER_OK;
}

int ReadSourceFile (Assembler *assembler, char *inputFileName)
{
    assert (assembler);
    assert (inputFileName);

    DEBUG_LOG ("%s", "ReadSourceFile()");

    int status = TextCtor (inputFileName, &assembler->sourceText);
    if (status != 0)
        return ASSEMBLER_COMMON_ERROR;

    DEBUG_LOG ("buffer:\n\"%s\"\n", assembler->sourceText.buffer);

    assembler->sourceFileName    = inputFileName;
    assembler->instructionsCapacity = 0;

    StrReplace (assembler->sourceText.buffer, "\n;", '\0');

    return ASSEMBLER_OK;
}

int AllocateBytecode (Assembler *assembler)
{
    assert (assembler);

    assembler->bytecode = (int *) calloc (assembler->instructionsCapacity, sizeof (int));
    if (assembler->bytecode == nullptr)
        return ASSEMBLER_COMMON_ERROR;

    return ASSEMBLER_OK;
}

int AssembleProgram (Assembler *assembler)
{
    assert (assembler);

    int status = RunFirstPass (assembler);
    if (status != ASSEMBLER_OK)
    {
        ERROR ("%s", "First pass failed");
        return status;
    }

    status = AllocateBytecode (assembler);
    if (status != ASSEMBLER_OK)
    {
        ERROR ("%s", "Failed to allocate bytecode buffer");
        return status;
    }

    status = RunFinalPass (assembler);
    if (status != ASSEMBLER_OK)
    {
        ERROR ("%s", "Final pass failed");
        return status;
    }

    return ASSEMBLER_OK;
}

int WriteBinaryOutput (const char *outputFileName, Assembler *assembler)
{
    assert (assembler);
    assert (outputFileName);

    FILE *outputFile = fopen (outputFileName, "w");
    if (outputFile == nullptr)
    {
        perror ("Error opening output file");
        return ASSEMBLER_COMMON_ERROR;
    }

    int status = WriteFileHeader (outputFile, assembler);
    if (status != ASSEMBLER_OK)
        ERROR_PRINT ("Error writing header to %s", outputFileName);

    status = WriteFileBody (outputFile, assembler);
    if (status != ASSEMBLER_OK)
        ERROR_PRINT ("Error writing body to %s", outputFileName);

    fclose (outputFile);

    DEBUG_LOG ("%s", "WriteBinaryOutput() OK");
    return ASSEMBLER_OK;
}

int PrintAssemblerError (int error)
{
    DEBUG_LOG ("Assembler error = %d\n", error);

    if (error == ASSEMBLER_OK) return 0;

    fprintf (stderr, "%s", RED_BOLD_COLOR);

    if (error & ASSEMBLER_UNKNOWN_COMMAND)               fprintf (stderr, "%s", "Unknown command in source file\n");
    if (error & ASSEMBLER_MISSING_ARGUMENT)              fprintf (stderr, "%s", "Missing argument for command that requires one\n");
    if (error & ASSEMBLER_TRASH_SYMBOLS)                 fprintf (stderr, "%s", "Unexpected symbols after command\n");
    if (error & ASSEMBLER_INVALID_REGISTER_NAME)         fprintf (stderr, "%s", "Invalid register name — expected e.g. \"RAX\"\n");
    if (error & ASSEMBLER_INVALID_REGISTER_ADDRESS_NAME) fprintf (stderr, "%s", "Invalid memory address register — expected e.g. \"[RAX]\"\n");
    if (error & ASSEMBLER_DUPLICATE_LABEL)               fprintf (stderr, "%s", "Duplicate label definition\n");
    if (error & ASSEMBLER_INVALID_LABEL)                 fprintf (stderr, "%s", "Reference to undefined label\n");
    if (error & ASSEMBLER_COMMON_ERROR)                  fprintf (stderr, "%s", "Internal assembler error\n");

    fprintf (stderr, "%s", COLOR_END);

    return ASSEMBLER_OK;
}

int DestroyAssembler (Assembler *assembler)
{
    assert (assembler);

    TextDtor (&assembler->sourceText);

    free (assembler->bytecode);
    assembler->bytecode = nullptr;

    assembler->instructionsCapacity = 0;
    assembler->sourceFileName       = nullptr;
    assembler->currentLineNumber    = 0;

    for (size_t i = 0; i < SPU_MAX_LABELS_COUNT; i++)
        assembler->labels[i].addressValue = 0;

    if (assembler->listingFile)
    {
        fclose (assembler->listingFile);
        assembler->listingFile = nullptr;
    }

    return ASSEMBLER_OK;
}


static int RunFirstPass (Assembler *assembler)
{
    assert (assembler);

    DEBUG_PRINT ("%s", "========== FIRST PASS ==========\n");

    for (size_t i = 0; assembler->sourceText.lines[i].start != nullptr; i++)
    {
        int status = ProcessLine (assembler, i, FIRST_PASS);
        if (status != ASSEMBLER_OK)
            return status;
    }

    assembler->instructionsCapacity += assembler->instructionPointer + 1;

    return ASSEMBLER_OK;
}

static int RunFinalPass (Assembler *assembler)
{
    assert (assembler);

    DEBUG_PRINT ("%s", "========== FINAL PASS ==========\n");

    assembler->instructionPointer = 0;

    for (size_t i = 0; assembler->sourceText.lines[i].start != nullptr; i++)
    {
        int status = ProcessLine (assembler, i, FINAL_PASS);
        if (status != ASSEMBLER_OK)
            return status;
    }

    return ASSEMBLER_OK;
}


static int ProcessLine (Assembler *assembler, size_t lineIdx, AssemblyPass pass)
{
    assert (assembler);

    assembler->currentLineNumber = lineIdx + 1;

    char *linePtr = assembler->sourceText.lines[lineIdx].start;
    linePtr = SkipSpaces (linePtr);

    if (linePtr[0] == '\0')
        return ASSEMBLER_OK;

    if (linePtr[0] == ':')
    {
        int status = RegisterLabel (assembler, &linePtr);
        if (status != ASSEMBLER_OK)
            return status;

        return CheckForTrailingGarbage (assembler, linePtr);
    }

    const Command *command = LookupCommand (&linePtr);
    if (command == nullptr)
    {
        ERROR_PRINT ("%s:%lu Unknown command \"%s\"",
                     assembler->sourceFileName, assembler->currentLineNumber, linePtr);
        return ASSEMBLER_UNKNOWN_COMMAND;
    }

    DEBUG_LOG ("command bytecode = %d;", command->bytecode);
    DEBUG_LOG ("linePtr = \"%s\";",      linePtr);

    if (pass == FIRST_PASS)
    {
        assembler->instructionPointer += 1;
        assembler->instructionPointer += (command->allowedArgumentTypes != ARG_TYPE_NONE);
        return ASSEMBLER_OK;
    }

    const size_t instructionStart = assembler->instructionPointer;

    int status = EmitCommand (assembler, command, &linePtr);
    if (status != ASSEMBLER_OK)
        return status;

    status = CheckForTrailingGarbage (assembler, linePtr);
    if (status != ASSEMBLER_OK)
        return status;

    return EmitListingLine (assembler, instructionStart);
}


static int EmitCommand (Assembler *assembler, const Command *command, char **linePtr)
{
    assert (assembler);
    assert (command);
    assert (linePtr && *linePtr);

    int status = GrowBytecodeIfNeeded (assembler);
    if (status != ASSEMBLER_OK)
        return status;

    assembler->bytecode[assembler->instructionPointer] = command->bytecode;
    assembler->instructionPointer++;

    if (command->allowedArgumentTypes == ARG_TYPE_NONE)
        return ASSEMBLER_OK;

    Argument argument = {};

    status = ParseArgument (assembler, &argument, linePtr);
    if (status != ASSEMBLER_OK)
    {
        ERROR_PRINT ("%s:%lu Missing argument for command \"%s\"",
                     assembler->sourceFileName, assembler->currentLineNumber, command->name);
        return status;
    }

    if (!(argument.type & command->allowedArgumentTypes))
    {
        ERROR_PRINT ("%s:%lu Wrong argument type for \"%s\"",
                     assembler->sourceFileName, assembler->currentLineNumber, command->name);
        return ASSEMBLER_WRONG_ARGUMENT_TYPE;
    }

    assembler->bytecode[assembler->instructionPointer] = argument.value;
    assembler->instructionPointer++;

    return ASSEMBLER_OK;
}


static int ParseArgument (Assembler *assembler, Argument *argument, char **linePtr)
{
    assert (assembler);
    assert (argument);
    assert (linePtr && *linePtr);

    *linePtr = SkipSpaces (*linePtr);

    DEBUG_LOG ("*linePtr = \"%s\";", *linePtr);

    if ((*linePtr)[0] == ':')
    {
        *linePtr += 1;

        argument->type  = ARG_TYPE_LABEL;
        argument->value = UNINITIALIZED_LABEL_VALUE;

        size_t wordLen = GetWordLen (*linePtr, " ");
        (*linePtr)[wordLen] = '\0';
        char *labelName = *linePtr;
        *linePtr += wordLen + 1;

        unsigned long labelHash = HashDjb2 (labelName);
        int idx = LookupLabel (assembler, labelName, labelHash);

        if (idx < 0)
        {
            ERROR_PRINT ("%s:%lu Undefined label \":%.*s\"",
                         assembler->sourceFileName, assembler->currentLineNumber,
                         (int)wordLen, labelName);
            return ASSEMBLER_INVALID_LABEL;
        }

        argument->value = (int)assembler->labels[idx].addressValue;
        return ASSEMBLER_OK;
    }

    int bytesConsumed = 0;
    if (sscanf (*linePtr, "%d%n", &argument->value, &bytesConsumed) == 1)
    {
        *linePtr += bytesConsumed;
        argument->type = ARG_TYPE_NUMBER;
        return ASSEMBLER_OK;
    }

    if (DecodeRegister (*linePtr, &bytesConsumed, &argument->value) == ASSEMBLER_OK)
    {
        *linePtr += bytesConsumed;
        argument->type = ARG_TYPE_REGISTER;
        return ASSEMBLER_OK;
    }

    if (DecodeMemoryRegister (*linePtr, &bytesConsumed, &argument->value) == ASSEMBLER_OK)
    {
        *linePtr += bytesConsumed;
        argument->type = ARG_TYPE_MEMORY_ADDRESS;
        return ASSEMBLER_OK;
    }

    argument->type  = ARG_TYPE_NONE;
    argument->value = DEFAULT_ARGUMENT_VALUE;

    return ASSEMBLER_MISSING_ARGUMENT;
}


static int RegisterLabel (Assembler *assembler, char **linePtr)
{
    assert (assembler);
    assert (linePtr && *linePtr);

    if ((*linePtr)[0] != ':')
    {
        ERROR_PRINT ("%s:%lu Expected ':' before label name",
                     assembler->sourceFileName, assembler->currentLineNumber);
        return ASSEMBLER_INVALID_LABEL;
    }
    *linePtr += 1;

    if (assembler->labelsCount >= SPU_MAX_LABELS_COUNT)
    {
        ERROR_PRINT ("%s:%lu Max label count (%lu) exceeded",
                     assembler->sourceFileName, assembler->currentLineNumber,
                     SPU_MAX_LABELS_COUNT);
        return ASSEMBLER_INVALID_LABEL;
    }

    size_t wordLen = GetWordLen (*linePtr, " ");
    (*linePtr)[wordLen] = '\0';
    char *labelName = *linePtr;
    *linePtr += wordLen;

    unsigned long labelHash = HashDjb2 (labelName);
    int idx = LookupLabel (assembler, labelName, labelHash);

    if (idx >= 0)
    {
        assembler->labels[idx].name         = labelName;
        assembler->labels[idx].hash         = labelHash;
        assembler->labels[idx].addressValue = (ssize_t)assembler->instructionPointer;
    }
    else
    {
        size_t slot = assembler->labelsCount;
        assembler->labels[slot].name         = labelName;
        assembler->labels[slot].hash         = labelHash;
        assembler->labels[slot].addressValue = (ssize_t)assembler->instructionPointer;
        assembler->labelsCount++;
    }

    return ASSEMBLER_OK;
}

static int LookupLabel (Assembler *assembler, const char *name, unsigned long hash)
{
    assert (assembler);
    assert (name);

    for (size_t i = 0; i < SPU_MAX_LABELS_COUNT; i++)
    {
        if (assembler->labels[i].hash == hash &&
            assembler->labels[i].name != nullptr &&
            strcmp (assembler->labels[i].name, name) == 0)
        {
            return (int)i;
        }
    }

    return -1;
}


static const Command *LookupCommand (char **linePtr)
{
    assert (linePtr && *linePtr);

    size_t wordLen = GetWordLen (*linePtr, " ");

    for (size_t i = 0; i < COMMANDS_COUNT; i++)
    {
        if (wordLen != COMMANDS_TABLE[i].nameLength - 1) continue;

        if (strncmp (*linePtr, COMMANDS_TABLE[i].name, wordLen) == 0)
        {
            *linePtr += wordLen + 1;
            return &COMMANDS_TABLE[i];
        }
    }

    return nullptr;
}


static int DecodeMemoryRegister (char *name, int *bytesConsumed, int *bytecode)
{
    assert (name);
    assert (bytesConsumed);
    assert (bytecode);

    if (strlen (name) < REGISTER_NAME_LEN + 2) return ASSEMBLER_INVALID_REGISTER_ADDRESS_NAME;
    if (name[0] != '[')                         return ASSEMBLER_INVALID_REGISTER_ADDRESS_NAME;
    if (name[4] != ']')                         return ASSEMBLER_INVALID_REGISTER_ADDRESS_NAME;

    int innerBytes = 0;
    int status = DecodeRegister (name + 1, &innerBytes, bytecode);
    if (status != ASSEMBLER_OK)
        return status;

    *bytesConsumed = innerBytes + 2; 

    return ASSEMBLER_OK;
}

static int DecodeRegister (char *name, int *bytesConsumed, int *bytecode)
{
    assert (name);
    assert (bytesConsumed);
    assert (bytecode);

    if (strlen (name) < REGISTER_NAME_LEN) return ASSEMBLER_INVALID_REGISTER_NAME;
    if (name[0] != 'R')                    return ASSEMBLER_INVALID_REGISTER_NAME;
    if (name[2] != 'X')                    return ASSEMBLER_INVALID_REGISTER_NAME;

    int registerIndex = name[1] - 'A';

    if (registerIndex < 0)                             return ASSEMBLER_INVALID_REGISTER_NAME;
    if ((size_t)registerIndex >= NUMBER_OF_REGISTERS)  return ASSEMBLER_INVALID_REGISTER_NAME;

    *bytecode      = registerIndex;
    *bytesConsumed = REGISTER_NAME_LEN + 2;

    return ASSEMBLER_OK;
}


static int WriteFileHeader (FILE *outputFile, Assembler *assembler)
{
    assert (outputFile);
    assert (assembler);

    if (fprintf (outputFile, "%lu ", MY_ASM_VERSION) < 0)
        return ASSEMBLER_COMMON_ERROR;

    if (fprintf (outputFile, "%lu ", assembler->instructionsCapacity - 1) < 0)
        return ASSEMBLER_COMMON_ERROR;

    return ASSEMBLER_OK;
}

static int WriteFileBody (FILE *outputFile, Assembler *assembler)
{
    assert (outputFile);
    assert (assembler);

    for (size_t i = 0; i < assembler->instructionsCapacity; i++)
    {
        if (fprintf (outputFile, "%d ", assembler->bytecode[i]) < 0)
            return ASSEMBLER_COMMON_ERROR;
    }

    return ASSEMBLER_OK;
}


static int EmitListingLine (Assembler *assembler, size_t instructionStart)
{
    assert (assembler);

    if (fprintf (assembler->listingFile, "%04lu \t ", instructionStart) < 0)
        return ASSEMBLER_COMMON_ERROR;

    size_t spaceAlign = 4 + 1 + 4 + 1;

    for (size_t i = instructionStart; i < assembler->instructionPointer; i++)
    {
        int written = fprintf (assembler->listingFile, "%4d ", assembler->bytecode[i]);
        if (written < 0)
            return ASSEMBLER_COMMON_ERROR;
        spaceAlign -= (size_t)written;
    }

    if (PrintSymbols (assembler->listingFile, spaceAlign, ' ') != ASSEMBLER_OK)
        return ASSEMBLER_COMMON_ERROR;

    const char *sourceLine = assembler->sourceText.lines[assembler->currentLineNumber - 1].start;
    if (fprintf (assembler->listingFile, "\t %s\n", sourceLine) < 0)
        return ASSEMBLER_COMMON_ERROR;

    return ASSEMBLER_OK;
}


static int CheckForTrailingGarbage (Assembler *assembler, char *linePtr)
{
    assert (assembler);

    const line_t &line = assembler->sourceText.lines[assembler->currentLineNumber - 1];

    while (*linePtr != '\0' && linePtr < line.start + line.len)
    {
        if (!isspace ((unsigned char)*linePtr))
        {
            ERROR_PRINT ("%s:%lu Unexpected trailing symbols: \"%s\"",
                         assembler->sourceFileName, assembler->currentLineNumber, linePtr);
            return ASSEMBLER_TRASH_SYMBOLS;
        }
        linePtr++;
    }

    return ASSEMBLER_OK;
}

static int GrowBytecodeIfNeeded (Assembler *assembler)
{
    assert (assembler);

    if (assembler->instructionPointer + 1 < assembler->instructionsCapacity)
        return ASSEMBLER_OK;

    ERROR ("%s",
           "GrowBytecodeIfNeeded() triggered — "
           "bytecode capacity was underestimated in the first pass");

    assembler->instructionsCapacity *= 2;

    int *grown = (int *)realloc (assembler->bytecode,
                                 sizeof (int) * assembler->instructionsCapacity);
    if (grown == nullptr)
    {
        perror ("realloc failed for bytecode");
        return ASSEMBLER_COMMON_ERROR;
    }

    assembler->bytecode = grown;

    return ASSEMBLER_OK;
}