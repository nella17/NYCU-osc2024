#pragma once

#include <type_traits>

#include "arm.hpp"
#include "mm/mm.hpp"
#include "util.hpp"

extern char __upper_PGD[];
extern char __upper_PUD[];
extern char __upper_end[];

constexpr uint64_t ADDRESS_SPACE_TAG = 0xFFFF000000000000;
constexpr uint64_t KERNEL_SPACE = 0xFFFF000000000000;
constexpr uint64_t USER_SPACE = 0;

template <typename T>
inline uint64_t lower_addr(T x) {
  return (uint64_t)x & (~ADDRESS_SPACE_TAG);
}

template <typename T,
          typename R = std::conditional_t<sizeof(T) == sizeof(void*), T, void*>>
inline R va2pa(T x) {
  return (R)(lower_addr(x));
}
template <typename T,
          typename R = std::conditional_t<sizeof(T) == sizeof(void*), T, void*>>
inline R pa2va(T x) {
  return (R)(lower_addr(x) | KERNEL_SPACE);
}

template <typename T>
uint64_t addressSpace(T x) {
  return (uint64_t)x & ADDRESS_SPACE_TAG;
}
template <typename T>
inline bool isKernelSpace(T x) {
  return addressSpace(x) == KERNEL_SPACE;
}
template <typename T>
inline bool isUserSpace(T x) {
  return addressSpace(x) == USER_SPACE;
}

struct PageTable;
constexpr uint64_t TABLE_SIZE_4K = 512;
constexpr uint64_t PTE_ENTRY_SIZE = PAGE_SIZE;
constexpr uint64_t PMD_ENTRY_SIZE = PTE_ENTRY_SIZE * TABLE_SIZE_4K;
constexpr uint64_t PUD_ENTRY_SIZE = PMD_ENTRY_SIZE * TABLE_SIZE_4K;
constexpr uint64_t PGD_ENTRY_SIZE = PUD_ENTRY_SIZE * TABLE_SIZE_4K;
constexpr uint64_t ENTRY_SIZE[] = {PGD_ENTRY_SIZE, PUD_ENTRY_SIZE,
                                   PMD_ENTRY_SIZE, PTE_ENTRY_SIZE};
constexpr int PGD_LEVEL = 0, PUD_LEVEL = 1, PMD_LEVEL = 2, PTE_LEVEL = 3;

struct PageTableEntry {
  uint64_t upper_atributes : 10 = 0;
  bool UXN : 1 = false;
  bool PXN : 1 = false;
  bool Contiguous : 1 = false;
  uint64_t output_address : 36 = 0;
  bool nG : 1 = false;
  bool AF : 1 = false;
  bool SH : 1 = false;
  bool RDONLY : 1 = false;
  bool USER : 1 = true;
  bool NS : 1 = false;
  uint64_t AttrIdx : 3 = MAIR_IDX_NORMAL_NOCACHE;
  uint64_t type : 2 = PD_INVALID;

  void print() const;

  bool isInvalid() const {
    return (type & 1) == 0;
  }
  bool isEntry() const {
    return type == PD_BLOCK or type == PD_ENTRY;
  }
  bool isTable() const {
    return type == PD_TABLE;
  }

  void* addr() const {
    return (void*)(output_address * PAGE_SIZE);
  }
  void set_addr(void* addr, uint64_t t) {
    AF = true;
    output_address = (uint64_t)va2pa(addr) / PAGE_SIZE;
    type = t;
  }

  PageTable* table() const {
    return (PageTable*)pa2va(addr());
  }
  void set_table(PageTable* table) {
    set_addr((void*)table, PD_TABLE);
  }
};
static_assert(sizeof(PageTableEntry) == sizeof(uint64_t));

struct PageTable {
  PageTableEntry entries[TABLE_SIZE_4K];
  PageTable();
  PageTable(PageTableEntry entry, int level);
  PageTable* copy();
  PageTable(PageTable* table);
  ~PageTable();
  PageTableEntry& walk(uint64_t start, int level, uint64_t va_start,
                       int va_level);
  using CB = void(PageTableEntry& entry, uint64_t start, int level);
  void walk(uint64_t start, int level, uint64_t va_start, uint64_t va_end,
            int va_level, CB cb_entry);
  template <typename T, typename U>
  void walk(T va_start, U va_end, CB cb_entry) {
    walk(USER_SPACE, PGD_LEVEL, (uint64_t)va_start, (uint64_t)va_end, PTE_LEVEL,
         cb_entry);
  }

  void traverse(uint64_t start, int level, CB cb_entry, CB cb_table = nullptr);
  void traverse(CB cb_entry, CB cb_table = nullptr) {
    return traverse(USER_SPACE, PGD_LEVEL, cb_entry, cb_table);
  }

  void print(uint64_t start = USER_SPACE, int level = PGD_LEVEL,
             const char* name = "PageTable");
};

static_assert(sizeof(PageTable) == PAGE_SIZE);

void map_kernel_as_normal(char* ktext_beg, char* ktext_end);
