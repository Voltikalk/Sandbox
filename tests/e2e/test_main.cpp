#define SDL_MAIN_HANDLED

#include "e2e_framework.hpp"
#include <iostream>
#include <iomanip>
#include <string>
#include <string_view>
#include <vector>
#include <map>
#include <chrono>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef min
#undef max
#endif

using namespace sandbox::test;

namespace {

// Enable Windows 10/11 Virtual Terminal Processing for ANSI colors
void EnableVirtualTerminal() {
#if defined(_WIN32)
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE) {
        DWORD dwMode = 0;
        if (GetConsoleMode(hOut, &dwMode)) {
            dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(hOut, dwMode);
        }
    }
#endif
}

struct CommandLineOptions {
    bool runTier1{false};
    bool runTier2{false};
    bool runTier3{false};
    bool runTier4{false};
    bool runTier5{false};
    bool verbose{false};
    bool listOnly{false};
    bool showHelp{false};
    bool hasSyntaxError{false};
    std::string errorMessage{""};
    std::string filter{""};
};

void PrintUsage(const char* exeName) {
    std::cout << "Usage: " << exeName << " [options]\n\n"
              << "Options:\n"
              << "  --tier=1           Run only Tier 1: Feature Coverage Tests\n"
              << "  --tier=2           Run only Tier 2: Boundary & Corner Case Tests\n"
              << "  --tier=3           Run only Tier 3: Pairwise Cross-Feature Tests\n"
              << "  --tier=4           Run only Tier 4: Workload & Scenario Tests\n"
              << "  --tier=5           Run only Tier 5: Adversarial Hardening Tests (opt-in)\n"
              << "  --all              Run Tiers 1 to 4 (default if no tier specified)\n"
              << "  --verbose, -v      Enable detailed per-test logging and assertions\n"
              << "  --filter=<str>     Run only tests whose Suite.Name contains <str>\n"
              << "  --test=<str>       Alias for --filter=<str>\n"
              << "  --list             List all registered tests without running\n"
              << "  --help, -h         Show this help message\n";
}

CommandLineOptions ParseCommandLine(int argc, char* argv[]) {
    CommandLineOptions opts;
    bool explicitTierSelected = false;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg.rfind("--tier=", 0) == 0) {
            std::string_view val = arg.substr(7);
            if (val == "1") {
                opts.runTier1 = true;
                explicitTierSelected = true;
            } else if (val == "2") {
                opts.runTier2 = true;
                explicitTierSelected = true;
            } else if (val == "3") {
                opts.runTier3 = true;
                explicitTierSelected = true;
            } else if (val == "4") {
                opts.runTier4 = true;
                explicitTierSelected = true;
            } else if (val == "5") {
                opts.runTier5 = true;
                explicitTierSelected = true;
            } else {
                opts.hasSyntaxError = true;
                if (!opts.errorMessage.empty()) opts.errorMessage += "\n";
                opts.errorMessage += "Invalid tier argument: " + std::string(arg) + " (expected 1, 2, 3, 4, or 5)";
            }
        } else if (arg == "--tier") {
            opts.hasSyntaxError = true;
            if (!opts.errorMessage.empty()) opts.errorMessage += "\n";
            opts.errorMessage += "Missing tier value for '--tier'. Expected '--tier=N' (N in 1..5)";
        } else if (arg == "--all") {
            opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true;
            opts.runTier5 = false;
            explicitTierSelected = true;
        } else if (arg == "--verbose" || arg == "-v") {
            opts.verbose = true;
        } else if (arg == "--list") {
            opts.listOnly = true;
        } else if (arg == "--help" || arg == "-h") {
            opts.showHelp = true;
        } else if (arg.rfind("--filter=", 0) == 0) {
            opts.filter = std::string(arg.substr(9));
        } else if (arg.rfind("--test=", 0) == 0) {
            opts.filter = std::string(arg.substr(7));
        } else if (arg == "--filter" || arg == "--test") {
            opts.hasSyntaxError = true;
            if (!opts.errorMessage.empty()) opts.errorMessage += "\n";
            opts.errorMessage += "Missing argument for option: " + std::string(arg) + " (expected '" + std::string(arg) + "=<str>')";
        } else {
            opts.hasSyntaxError = true;
            if (!opts.errorMessage.empty()) opts.errorMessage += "\n";
            opts.errorMessage += "Unknown argument: " + std::string(arg);
        }
    }

    if (!explicitTierSelected) {
        opts.runTier1 = opts.runTier2 = opts.runTier3 = opts.runTier4 = true;
        opts.runTier5 = false;
    }

    return opts;
}

bool IsTierSelected(int tier, const CommandLineOptions& opts) {
    switch (tier) {
        case 1: return opts.runTier1;
        case 2: return opts.runTier2;
        case 3: return opts.runTier3;
        case 4: return opts.runTier4;
        case 5: return opts.runTier5;
        default: return false;
    }
}

bool MatchesFilter(const TestCase& test, const std::string& filter) {
    if (filter.empty()) return true;
    std::string fullName = test.suite + "." + test.name;
    return fullName.find(filter) != std::string::npos;
}

struct TierStats {
    int total{0};
    int passed{0};
    int failed{0};
    int crashed{0};
    double elapsedMs{0.0};
};

struct FailureRecord {
    std::string suite;
    std::string name;
    int tier{1};
    std::string reason;
    double elapsedMs{0.0};
};

} // anonymous namespace

// ============================================================================
// Built-in Sanity Test: Validates that harness assertions and fixture operate
// ============================================================================
E2E_TEST(HarnessSanity, AssertionsAndWorldFixture, 1) {
    // 1. Boolean comparisons
    ASSERT_TRUE(true);
    ASSERT_FALSE(false);
    ASSERT_EQ(true, true);
    ASSERT_EQ(false, false);
    ASSERT_NE(true, false);

    // 2. Character comparisons
    ASSERT_EQ('P', 'P');
    ASSERT_NE('A', 'B');

    // 3. Integer comparisons (same signedness)
    ASSERT_EQ(42, 42);
    ASSERT_NE(10, 20);

    // 4. Signed vs unsigned relational comparisons
    ASSERT_LT(-1, 1u);
    ASSERT_GT(10u, 5);
    ASSERT_GE(5u, 5);
    ASSERT_LE(5, 5u);

    // 5. Floating point comparisons
    ASSERT_FLOAT_NEAR(1.0f, 1.0f, 1e-6f);
    ASSERT_FLOAT_NEAR(3.14159f, 3.14150f, 0.001f);

    // 6. Relational comparisons
    ASSERT_GT(100, 50);
    ASSERT_LT(25, 75);
    ASSERT_GE(50, 50);
    ASSERT_LE(30, 30);

    // 7. World fixture verification & mutations
    TestWorldFixture fixture(64, 64);
    ASSERT_EQ(fixture.world.GetWidth(), 64);
    ASSERT_EQ(fixture.world.GetHeight(), 64);

    fixture.world.SetCell(10, 10, 1, 25.0f);
    ASSERT_EQ(fixture.world.GetCellMaterial(10, 10), 1);
    ASSERT_FLOAT_NEAR(fixture.world.GetCellTemperature(10, 10), 25.0f, 0.01f);

    // 8. Inspection utilities
    ASSERT_EQ(inspection::CountMaterial(fixture.world, 1), 1u);
    ASSERT_EQ(inspection::CountMaterialInBox(fixture.world, 1, 0, 0, 20, 20), 1u);
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char* argv[]) {
    EnableVirtualTerminal();

    CommandLineOptions opts = ParseCommandLine(argc, argv);
    if (opts.hasSyntaxError) {
        if (!opts.errorMessage.empty()) {
            std::cerr << opts.errorMessage << "\n\n";
        }
        PrintUsage(argv[0]);
        return 2;
    }

    if (opts.showHelp) {
        PrintUsage(argv[0]);
        return 0;
    }

    const auto& allTests = TestRegistry::Instance().GetTests();

    if (opts.listOnly) {
        std::cout << "Registered E2E Tests (" << allTests.size() << " total):\n";
        for (const auto& test : allTests) {
            std::cout << "  [Tier " << test.tier << "] " << test.suite << "." << test.name
                      << " (" << test.file << ":" << test.line << ")\n";
        }
        return 0;
    }

    std::vector<const TestCase*> selectedTests;
    for (const auto& test : allTests) {
        if (!IsTierSelected(test.tier, opts)) continue;
        if (!MatchesFilter(test, opts.filter)) continue;
        selectedTests.push_back(&test);
    }

    std::cout << "============================================================\n";
    std::cout << "  SANDBOX 2D E2E TEST RUNNER\n";
    std::cout << "  Total Registered: " << allTests.size() << " | Selected: " << selectedTests.size() << "\n";
    std::cout << "  Tiers Active: ";
    if (opts.runTier1) std::cout << "T1 ";
    if (opts.runTier2) std::cout << "T2 ";
    if (opts.runTier3) std::cout << "T3 ";
    if (opts.runTier4) std::cout << "T4 ";
    if (opts.runTier5) std::cout << "T5 ";
    std::cout << "\n============================================================\n\n";

    if (selectedTests.empty()) {
        std::cout << "[NOTICE] No tests match the active criteria or filter.\n";
        return 0;
    }

    std::map<int, TierStats> tierStats;
    std::vector<FailureRecord> failures;
    int totalPassed = 0;
    int totalFailed = 0;
    int totalCrashed = 0;

    auto globalStart = std::chrono::high_resolution_clock::now();

    for (const auto* test : selectedTests) {
        if (opts.verbose) {
            std::cout << "\033[36m[ RUN      ]\033[0m [" << test->suite << "." << test->name << "] (Tier " << test->tier << ")\n";
        }

        auto testStart = std::chrono::high_resolution_clock::now();
        bool passed = false;
        std::string failureDetail;

        try {
            test->testFunc();
            passed = true;
        } catch (const AssertionFailureException& e) {
            passed = false;
            failureDetail = e.what();
            totalFailed++;
        } catch (const std::exception& e) {
            passed = false;
            failureDetail = std::string("Unhandled std::exception: ") + e.what();
            totalCrashed++;
        } catch (...) {
            passed = false;
            failureDetail = "Unhandled non-standard exception / crash";
            totalCrashed++;
        }

        auto testEnd = std::chrono::high_resolution_clock::now();
        double elapsedMs = std::chrono::duration<double, std::milli>(testEnd - testStart).count();

        auto& stats = tierStats[test->tier];
        stats.total++;
        stats.elapsedMs += elapsedMs;

        if (passed) {
            totalPassed++;
            stats.passed++;
            if (opts.verbose) {
                std::cout << "\033[32m[       OK ]\033[0m [" << test->suite << "." << test->name << "] ("
                          << std::fixed << std::setprecision(2) << elapsedMs << " ms)\n";
            }
        } else {
            stats.failed++;
            std::cout << "\033[31m[  FAILED  ]\033[0m [" << test->suite << "." << test->name << "] (Tier " << test->tier
                      << ", " << std::fixed << std::setprecision(2) << elapsedMs << " ms)\n";
            std::cout << "             " << failureDetail << "\n";
            failures.push_back({test->suite, test->name, test->tier, failureDetail, elapsedMs});
        }
    }

    auto globalEnd = std::chrono::high_resolution_clock::now();
    double totalElapsedMs = std::chrono::duration<double, std::milli>(globalEnd - globalStart).count();

    // Summary Reporting
    std::cout << "\n============================================================\n";
    std::cout << "  E2E TEST EXECUTION SUMMARY\n";
    std::cout << "============================================================\n";
    for (int tier = 1; tier <= 5; ++tier) {
        if (tierStats.count(tier)) {
            const auto& s = tierStats[tier];
            std::cout << "  Tier " << tier << ": " << s.passed << "/" << s.total << " passed ("
                      << (s.failed == 0 && s.crashed == 0 ? "\033[32mPASSED\033[0m" : "\033[31mFAILED\033[0m") << ") in "
                      << std::fixed << std::setprecision(2) << s.elapsedMs << " ms\n";
        }
    }
    std::cout << "------------------------------------------------------------\n";
    std::cout << "  Total Executed : " << selectedTests.size() << "\n";
    std::cout << "  Passed         : " << totalPassed << "\n";
    std::cout << "  Failed         : " << totalFailed << "\n";
    std::cout << "  Crashed        : " << totalCrashed << "\n";
    std::cout << "  Total Duration : " << std::fixed << std::setprecision(2) << totalElapsedMs << " ms\n";
    std::cout << "============================================================\n";

    if (!failures.empty()) {
        std::cout << "\n\033[31mFailures Summary:\033[0m\n";
        for (const auto& f : failures) {
            std::cout << "  - [Tier " << f.tier << "] " << f.suite << "." << f.name << ": " << f.reason << "\n";
        }
        std::cout << "============================================================\n";
        return 1;
    }

    std::cout << "  \033[32mSTATUS: 100% OF TESTS PASSED\033[0m\n";
    std::cout << "============================================================\n";
    return 0;
}
