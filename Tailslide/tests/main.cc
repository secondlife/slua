// This file is originally part of the Luau programming language and is licensed under MIT License;

#define DOCTEST_CONFIG_IMPLEMENT
// Our calls to parseOption/parseFlag don't provide a prefix so set the prefix to the empty string.
#define DOCTEST_CONFIG_OPTIONS_PREFIX ""
#include "doctest.hh"
#include "lslmini.hh"

#ifdef _WIN32
# ifndef WIN32_LEAN_AND_MEAN
#   define WIN32_LEAN_AND_MEAN
# endif
# include <Windows.h> // IsDebuggerPresent
#endif

#ifdef __APPLE__
# include <sys/types.h>
# include <sys/sysctl.h>
#endif

#include <cassert>

using namespace Tailslide;

struct BoostLikeReporter : doctest::IReporter
{
    const doctest::TestCaseData *currentTest = nullptr;

    explicit BoostLikeReporter(const doctest::ContextOptions& in) {}

    // called when a query should be reported (listing test cases, printing the version, etc.)
    void report_query(const doctest::QueryData& qd) override
    {
        for (unsigned int i = 0; i < qd.num_data; ++i)
        {
            const doctest::TestCaseData& tc = *qd.data[i];

            fprintf(stderr, "%s/%s\n", tc.m_test_suite, tc.m_name);
        }

        fprintf(stderr, "Found %d tests.\n", int(qd.num_data));
    }

    // called when the whole test run starts/ends
    void test_run_start() override {}

    void test_run_end(const doctest::TestRunStats& ts) override {}

    // called when a test case is started (safe to cache a pointer to the input)
    void test_case_start(const doctest::TestCaseData& tc) override
    {
        currentTest = &tc;

        printf("Entering test suite \"%s\"\n", tc.m_test_suite);
        printf("Entering test case \"%s\"\n", tc.m_name);
    }

    // called when a test case has ended
    void test_case_end(const doctest::CurrentTestCaseStats& tc) override
    {
        assert(currentTest);

        printf("Leaving test case \"%s\"\n", currentTest->m_name);
        printf("Leaving test suite \"%s\"\n", currentTest->m_test_suite);

        currentTest = nullptr;
    }

    // called when an exception is thrown from the test case (or it crashes)
    void test_case_exception(const doctest::TestCaseException& e) override
    {
        assert(currentTest);

        printf("%s(%d): FATAL: Unhandled exception %s\n", currentTest->m_file.c_str(), currentTest->m_line, e.error_string.c_str());
    }

    // called whenever a subcase is entered/exited (noop)
    void test_case_reenter(const doctest::TestCaseData&) override {}
    void subcase_start(const doctest::SubcaseSignature&) override {}
    void subcase_end() override {}

    void log_assert(const doctest::AssertData& ad) override
    {
        if (!ad.m_failed)
            return;

        if (ad.m_decomp.size())
            printf("%s(%d): ERROR: %s (%s)\n", ad.m_file, ad.m_line, ad.m_expr, ad.m_decomp.c_str());
        else
            printf("%s(%d): ERROR: %s\n", ad.m_file, ad.m_line, ad.m_expr);
    }

    void log_message(const doctest::MessageData& md) override
    { //
        printf("%s(%d): ERROR: %s\n", md.m_file, md.m_line, md.m_string.c_str());
    }

    // called when a test case is skipped either because it doesn't pass the filters, has a skip decorator
    // or isn't in the execution range (between first and last) (safe to cache a pointer to the input)
    void test_case_skipped(const doctest::TestCaseData&) override {}
};


int main(int argc, char** argv)
{
    doctest::registerReporter<BoostLikeReporter>("boost", 0, true);

    doctest::Context context;
    context.setOption("no-version", true);
    context.applyCommandLine(argc, argv);

    if (doctest::parseFlag(argc, argv, "--list_content"))
    {
        const char *ltc[] = {argv[0], "--list-test-cases"};
        context.applyCommandLine(2, ltc);
    }

    doctest::String filter;
    if (doctest::parseOption(argc, argv, "--run_test", &filter) && filter[0] == '=')
    {
        const char *f = filter.c_str() + 1;
        const char *s = strchr(f, '/');

        if (s)
        {
            context.addFilter("test-suite", std::string(f, s).c_str());
            context.addFilter("test-case", s + 1);
        }
        else
        {
            context.addFilter("test-suite", f);
        }
    }

  tailslide_init_builtins(nullptr);
    int result = context.run();
    if (doctest::parseFlag(argc, argv, "--help") || doctest::parseFlag(argc, argv, "-h"))
    {
        printf("Have fun!\n");
    }
    return result;
}
