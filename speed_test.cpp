#include <iostream>
#include <chrono>

#define ADD_LOOP(i, n, v)       \
asm volatile (                  \
    "movq %1, %%rcx      ;"      \
    "movq %2, %%rax     ;"      \
    "movq $0, %%rbx     ;"      \
    "for:               ;"      \
    "addq %%rax, %%rbx  ;"      \
    "decq %%rcx          ;"      \
    "jnz for            ;"      \
    "movq %%rbx, %0     ;"      \
    : "=x"(v)                   \
    : "x"(i), "x"(n)            \
    : "%rcx", "%rax", "%rbx"     \
);

int main() {
    uint64_t iter(1000000);
    uint64_t num(5);
    uint64_t val(0);

    auto start = std::chrono::high_resolution_clock::now();

    ADD_LOOP(iter, num, val)

    auto finish = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(finish-start).count();

    uint64_t val2(0);
    uint64_t i(0);

    auto start2 = std::chrono::high_resolution_clock::now();

    for(i = 0; i < iter; i++) {
        val2 += num;
    }

    auto finish2 = std::chrono::high_resolution_clock::now();
    auto elapsed2 = std::chrono::duration_cast<std::chrono::nanoseconds>(finish2-start2).count();

    if(val == val2) {
        std::cout << "same result: " << val << std::endl;
    } else {
        std::cout << "different results: " << val << ", " << val2 << std::endl;
    }

    elapsed /= 1000;
    elapsed2 /= 1000;

    std::cout << std::endl;
    std::cout << "jmp loop: " << elapsed << "ms" << std::endl;
    std::cout << "for loop: " << elapsed2 << "ms" << std::endl;

    return 0;
}

// same result: 5000000
// 
// jnz loop: 200ms
// for loop: 1696ms

