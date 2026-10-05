// HydrolixQt1.cpp : Defines the entry point for the application.
//

#include "HydrolixQt1.h"
#include "CommonValuesTests.hpp"
#include "ComparisonBenchmarks.hpp"
#include <exception>
#include <iostream>

using namespace std;

int main() {
    try {
        runCommonValuesTests();
        runComparisonBenchmarks();
        return 0;
    }
    catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}