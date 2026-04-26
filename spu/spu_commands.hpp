#ifndef K_SPU_COMMANDS_HPP
#define K_SPU_COMMANDS_HPP

#include <cmath>
#include <cstdio>
#include <cstdlib>

inline int Spu::GetOperands(int* first, int* second) {
  if (second != nullptr) {
    int err = dataStack_.Pop(second);
    if (err != STACK_OK) {
      return RE_NOT_ENOGUH_ELEMENTS_ON_STACK;
    }
  }
  if (first != nullptr) {
    int err = dataStack_.Pop(first);
    if (err != STACK_OK) {
      return RE_NOT_ENOGUH_ELEMENTS_ON_STACK;
    }
  }
  return RE_OK;
}

inline int Spu::DoPush() {
  int val = FetchBytecode();
  if (val == RE_UKNOWN_BYTECODE) {
    return RE_UKNOWN_BYTECODE;
  }
  dataStack_.Push(val);
  return RE_OK;
}

inline int Spu::DoPop() {
  int dummy;
  return dataStack_.Pop(&dummy);
}

inline int Spu::DoAdd() {
  int a, b;
  int err = GetOperands(&a, &b);
  if (err != RE_OK) {
    return err;
  }
  dataStack_.Push(a + b);
  return RE_OK;
}

inline int Spu::DoSub() {
  int a, b;
  int err = GetOperands(&a, &b);
  if (err != RE_OK) {
    return err;
  }
  dataStack_.Push(a - b);
  return RE_OK;
}

inline int Spu::DoDiv() {
  int a, b;
  int err = GetOperands(&a, &b);
  if (err != RE_OK) {
    return err;
  }
  if (b == 0) {
    return RE_DIVISION_BY_ZERO;
  }
  dataStack_.Push(a / b);
  return RE_OK;
}

inline int Spu::DoMul() {
  int a, b;
  int err = GetOperands(&a, &b);
  if (err != RE_OK) {
    return err;
  }
  dataStack_.Push(a * b);
  return RE_OK;
}

inline int Spu::DoSqrt() {
  int val;
  int err = dataStack_.Pop(&val);
  if (err != STACK_OK) {
    return RE_NOT_ENOGUH_ELEMENTS_ON_STACK;
  }
  if (val < 0) {
    return RE_SQRT_NEGATIVE_ARGUMENT;
  }
  dataStack_.Push(static_cast<int>(std::sqrt(val)));
  return RE_OK;
}

inline int Spu::DoOut() {
  int val;
  int err = dataStack_.Pop(&val);
  if (err != STACK_OK) {
    return RE_NOT_ENOGUH_ELEMENTS_ON_STACK;
  }
  std::printf("%d\n", val);
  return RE_OK;
}

inline int Spu::DoIn() {
  int val;
  if (std::scanf("%d", &val) != 1) {
    return RE_INVALID_INPUT;
  }
  dataStack_.Push(val);
  return RE_OK;
}

inline int Spu::DoPushr() {
  int reg = FetchBytecode();
  if (reg < 0 || reg >= NUMBER_OF_REGISTERS) {
    return RE_MISSING_ARGUMENT;
  }
  dataStack_.Push(regs_[reg]);
  return RE_OK;
}

inline int Spu::DoPopr() {
  int reg = FetchBytecode();
  if (reg < 0 || reg >= NUMBER_OF_REGISTERS) {
    return RE_MISSING_ARGUMENT;
  }
  int val;
  int err = dataStack_.Pop(&val);
  if (err != STACK_OK) {
    return RE_NOT_ENOGUH_ELEMENTS_ON_STACK;
  }
  regs_[reg] = val;
  return RE_OK;
}

inline int Spu::DoJmp() {
  int addr = FetchBytecode();
  if (addr < 0) {
    return RE_JMP_ARGUMENT_IS_NEGATIVE;
  }
  ip_ = static_cast<std::size_t>(addr);
  return RE_OK;
}

inline int Spu::CompareAndJump(int condition) {
  int addr = FetchBytecode();
  if (addr < 0) {
    return RE_JMP_ARGUMENT_IS_NEGATIVE;
  }
  int a, b;
  int err = GetOperands(&a, &b);
  if (err != RE_OK) {
    return err;
  }
  bool take = false;
  switch (condition) {
    case 0b001: take = (a < b);  break;
    case 0b010: take = (a <= b); break;
    case 0b100: take = (a > b);  break;
    case 0b101: take = (a >= b); break;
    case 0b110: take = (a == b); break;
    case 0b111: take = (a != b); break;
    default:    return RE_UKNOWN_BYTECODE;
  }
  if (take) {
    ip_ = static_cast<std::size_t>(addr);
  }
  return RE_OK;
}

inline int Spu::DoJb()  { return CompareAndJump(0b001); }
inline int Spu::DoJbe() { return CompareAndJump(0b010); }
inline int Spu::DoJa()  { return CompareAndJump(0b100); }
inline int Spu::DoJae() { return CompareAndJump(0b101); }
inline int Spu::DoJe()  { return CompareAndJump(0b110); }
inline int Spu::DoJne() { return CompareAndJump(0b111); }

inline int Spu::DoCall() {
  int addr = FetchBytecode();
  if (addr < 0) {
    return RE_CALL_ARGUMENT_IS_NEGATIVE;
  }
  callStack_.Push(static_cast<int>(ip_));
  ip_ = static_cast<std::size_t>(addr);
  return RE_OK;
}

inline int Spu::DoRet() {
  int addr;
  int err = callStack_.Pop(&addr);
  if (err != STACK_OK) {
    return RE_RET_VALUE_IS_NEGATIVE;
  }
  ip_ = static_cast<std::size_t>(addr);
  return RE_OK;
}

inline int Spu::DoPushm() {
  int addr;
  int err = dataStack_.Pop(&addr);
  if (err != STACK_OK) {
    return RE_NOT_ENOGUH_ELEMENTS_ON_STACK;
  }
  if (addr < 0 || static_cast<std::size_t>(addr) >= SPU_MAX_RAM_SIZE) {
    return RE_MISSING_ARGUMENT;
  }
  dataStack_.Push(ram_[addr]);
  return RE_OK;
}

inline int Spu::DoPopm() {
  int addr, val;
  int err = dataStack_.Pop(&addr);
  if (err != STACK_OK) {
    return RE_NOT_ENOGUH_ELEMENTS_ON_STACK;
  }
  err = dataStack_.Pop(&val);
  if (err != STACK_OK) {
    return RE_NOT_ENOGUH_ELEMENTS_ON_STACK;
  }
  if (addr < 0 || static_cast<std::size_t>(addr) >= SPU_MAX_RAM_SIZE) {
    return RE_MISSING_ARGUMENT;
  }
  ram_[addr] = val;
  return RE_OK;
}

inline int Spu::DoDraw() {
  for (std::size_t y = 0; y < SPU_VIDEO_HEIGHT; ++y) {
    for (std::size_t x = 0; x < SPU_VIDEO_WIDTH; ++x) {
      std::putchar(ram_[y * SPU_VIDEO_WIDTH + x] ? '*' : ' ');
    }
    std::putchar('\n');
  }
  return RE_OK;
}

inline int Spu::Run() {
  while (ip_ < bytecode_.size()) {
    int opcode = bytecode_[ip_++];
    if (opcode < 0 || opcode >= static_cast<int>(Opcode::COUNT)) {
      return RE_UKNOWN_BYTECODE;
    }
    int err = (this->*dispatch_[opcode])();
    if (err != RE_OK) {
      return err;
    }
  }
  return RE_OK;
}

const std::array<Spu::Handler, static_cast<std::size_t>(Opcode::COUNT)> Spu::dispatch_ = {
  &Spu::DoPush,
  &Spu::DoPop,
  &Spu::DoAdd,
  &Spu::DoSub,
  &Spu::DoDiv,
  &Spu::DoMul,
  &Spu::DoSqrt,
  &Spu::DoOut,
  &Spu::DoIn,
  &Spu::DoPushr,
  &Spu::DoPopr,
  &Spu::DoJmp,
  &Spu::DoJb,
  &Spu::DoJbe,
  &Spu::DoJa,
  &Spu::DoJae,
  &Spu::DoJe,
  &Spu::DoJne,
  &Spu::DoCall,
  &Spu::DoRet,
  &Spu::DoPushm,
  &Spu::DoPopm,
  &Spu::DoDraw
};

#endif