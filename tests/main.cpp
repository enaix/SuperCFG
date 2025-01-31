//
// Created by Flynn on 06.11.2024.
//

#include "tests/bnf.h"

int main()
{
    auto res = test_gbnf();
    if (!res) { std::cout << "main() : tests failed" << std::endl; return 1; }
    std::cout << "main() : passed" << std::endl;
    return 0;
}