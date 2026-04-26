#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "assembler.hpp"
#include "debug.hpp"
#include "file.hpp"

static const char *DEFAULT_OUTPUT_PATH = "../spu/out.spu";

int main (int argc, char **argv)
{
    if (argc != 2)
    {
        fprintf (stderr, "Usage: %s <source.asm>\n", argv[0]);
        return 1;
    }

    const char *inputFileName  = argv[1];
    const char *outputFileName = DEFAULT_OUTPUT_PATH;

    Assembler assembler = {};

    int status = InitializeAssembler (&assembler);
    if (status != ASSEMBLER_OK)
    {
        ERROR ("%s", "Failed to initialize assembler");
        return status;
    }

    status = ReadSourceFile (&assembler, argv[1]);
    if (status != ASSEMBLER_OK)
    {
        ERROR ("Failed to read source file \"%s\"", inputFileName);
        DestroyAssembler (&assembler);
        return status;
    }

    status = AssembleProgram (&assembler);
    if (status != ASSEMBLER_OK)
    {
        ERROR ("%s", "Assembly failed");
        PrintAssemblerError (status);
        DestroyAssembler (&assembler);
        return status;
    }

    status = WriteBinaryOutput (outputFileName, &assembler);
    if (status != ASSEMBLER_OK)
    {
        ERROR ("Failed to write output to \"%s\"", outputFileName);
        PrintAssemblerError (status);
        DestroyAssembler (&assembler);
        return status;
    }

    DestroyAssembler (&assembler);
    return 0;
}