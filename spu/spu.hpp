#ifndef K_SPU_HPP
#define K_SPU_HPP

#include "stack.hpp"
#include <array>
#include <cstddef>
#include <vector>

constexpr std::size_t SPU_VIDEO_WIDTH  = 50;
constexpr std::size_t SPU_VIDEO_HEIGHT = 20;
constexpr std::size_t SPU_MAX_RAM_SIZE = SPU_VIDEO_WIDTH * SPU_VIDEO_HEIGHT;
constexpr int NUMBER_OF_REGISTERS      = 4;

enum spuErrors {
  SPU_OK                  = 0,
  SPU_NULL_STRUCT         = 1 << 0,
  SPU_BYTECODE_NULL       = 1 << 1,
  SPU_BYTECODE_OVERFLOW   = 1 << 2,
  SPU_WRONG_VERSION       = 1 << 3,
  SPU_COMMON_ERROR        = 1 << 31
};

enum runtimeErrors {
  RE_OK                              = 0,
  RE_MISSING_ARGUMENT                = 1 << 0,
  RE_NOT_ENOGUH_ELEMENTS_ON_STACK    = 1 << 1,
  RE_DIVISION_BY_ZERO                = 1 << 2,
  RE_SQRT_NEGATIVE_ARGUMENT          = 1 << 3,
  RE_INVALID_INPUT                   = 1 << 4,
  RE_UKNOWN_BYTECODE                 = 1 << 5,
  RE_JMP_ARGUMENT_IS_NEGATIVE        = 1 << 6,
  RE_CALL_ARGUMENT_IS_NEGATIVE       = 1 << 7,
  RE_RET_VALUE_IS_NEGATIVE           = 1 << 8
};

enum class Opcode {
  PUSH, POP, ADD, SUB, DIV, MUL, SQRT, OUT, IN,
  PUSHR, POPR, JMP, JB, JBE, JA, JAE, JE, JNE,
  CALL, RET, PUSHM, POPM, DRAW,
  COUNT
};

class Spu {
 public:
  Spu();

  int LoadBytecode(const std::vector<int>& code);
  int Run();

 private:
  using Handler = int (Spu::*)();
  static const std::array<Handler, static_cast<std::size_t>(Opcode::COUNT)> dispatch_;

  int DoPush();
  int DoPop();
  int DoAdd();
  int DoSub();
  int DoDiv();
  int DoMul();
  int DoSqrt();
  int DoOut();
  int DoIn();
  int DoPushr();
  int DoPopr();
  int DoJmp();
  int DoJb();
  int DoJbe();
  int DoJa();
  int DoJae();
  int DoJe();
  int DoJne();
  int DoCall();
  int DoRet();
  int DoPushm();
  int DoPopm();
  int DoDraw();

  int GetOperands(int* first, int* second);
  int FetchBytecode();
  int CompareAndJump(int condition);

  Stack dataStack_;
  Stack callStack_;
  std::vector<int> bytecode_;
  std::size_t ip_;
  std::array<int, NUMBER_OF_REGISTERS> regs_;
  std::array<int, SPU_MAX_RAM_SIZE> ram_;
};

inline Spu::Spu() : ip_(0), regs_{}, ram_{} {}

inline int Spu::LoadBytecode(const std::vector<int>& code) {
  bytecode_ = code;
  ip_ = 0;
  return SPU_OK;
}

inline int Spu::FetchBytecode() {
  if (ip_ >= bytecode_.size()) {
    return RE_UKNOWN_BYTECODE;
  }
  return bytecode_[ip_++];
}

#include "spu_commands.hpp"

#endif