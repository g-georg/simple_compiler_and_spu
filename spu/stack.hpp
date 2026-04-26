#ifndef K_STACK_HPP
#define K_STACK_HPP

#include <cstddef>
#include <vector>

enum stackErrors {
  STACK_OK                              = 0,
  STACK_NULL_STRUCT                     = 1 << 0,
  STACK_NULL_DATA                       = 1 << 1,
  STACK_OVERFLOW                        = 1 << 2,
  STACK_BIG_CAPACITY                    = 1 << 3,
  STACK_STRUCT_CANARY_START_OVERWRITE   = 1 << 4,
  STACK_STRUCT_CANARY_END_OVERWRITE     = 1 << 5,
  STACK_DATA_CANARY_START_OVERWRITE     = 1 << 6,
  STACK_DATA_CANARY_END_OVERWRITE       = 1 << 7,
  STACK_POISON_VALUE_IN_DATA            = 1 << 8,
  STACK_WRONG_VALUE_IN_POISON           = 1 << 9,
  STACK_TRYING_TO_POP_FROM_EMPTY_STACK  = 1 << 10
};

class Stack {
 public:
  Stack() = default;
  explicit Stack(const Stack&) = delete;
  Stack& operator=(const Stack&) = delete;

  int Push(int value);
  int Pop(int* value);
  int Top(int* value) const;
  bool Empty() const;
  std::size_t Size() const;

 private:
  std::vector<int> data_;
};

inline int Stack::Push(int value) {
  data_.push_back(value);
  return STACK_OK;
}

inline int Stack::Pop(int* value) {
  if (data_.empty()) {
    return STACK_TRYING_TO_POP_FROM_EMPTY_STACK;
  }
  if (value != nullptr) {
    *value = data_.back();
  }
  data_.pop_back();
  return STACK_OK;
}

inline int Stack::Top(int* value) const {
  if (data_.empty()) {
    return STACK_TRYING_TO_POP_FROM_EMPTY_STACK;
  }
  if (value != nullptr) {
    *value = data_.back();
  }
  return STACK_OK;
}

inline bool Stack::Empty() const {
  return data_.empty();
}

inline std::size_t Stack::Size() const {
  return data_.size();
}

#endif