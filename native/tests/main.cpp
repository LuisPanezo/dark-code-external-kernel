#include "core_tests.h"
#include <iostream>
#include <exception>
int main() {try {std::cout<<dc::RunCoreTests()<<" native checks passed\n";return 0;}
    catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
