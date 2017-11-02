''' Provides a simple test framework for PINocchio.
Will identify programs and tests, running all tests using different
configurations, printing a result table in the end. '''

from shared.testenv import Example, Shell, PINOCCHIO_BINARY

SEPARATOR = "--- --- --- --- ---"

def header(name):
    ''' simple header used for separating sections '''
    _header = "\n" + SEPARATOR
    _header += "\n" + name
    _header += "\n" + SEPARATOR
    return _header

def test_examples_must_finish(_examples):
    ''' all tests must finish under 5 seconds with different number
    of threads '''

    print header("Test: Examples must finish")
    for ex in _examples:
        print ex.must_finish(5)

def test_examples_must_finish_pin(_examples):
    ''' all tests must finish under 10 seconds with different number
    of threads using PINocchio '''

    print header("Test: Examples must finish with pin")
    for ex in _examples:
        if ex.finishes:
            print ex.must_finish_pin(10)

def test_examples_perf(_examples):
    ''' execute all tests, that havent failed yet, with perf stat '''

    print header("Test: Examples with perf")
    for ex in _examples:
        if ex.finishes:
            print ex.perf(10)

def test_examples_perf_with_pin(_examples):
    ''' execute all tests, that havent failed yet, with perf stat and pin '''

    print header("Test: Examples with perf, pin and PINocchio")
    for ex in _examples:
        if ex.finishes_with_perf and ex.finishes_with_pin:
            print ex.pin_with_perf(20)

def print_result(_examples):
    ''' Print the result table, using information from all threads '''

    print header("Results")

    table = []
    for ex in _examples:
        table.append(ex.result())

    Example.print_table(Example.create_table_names(), table, 3)

if __name__ == "__main__":
    programs = Shell.search_programs()
    examples = Shell.create_examples()

    # Test 1
    test_examples_must_finish(examples)

    # Test 2
    missing = Shell.check_dependencies(programs, ["pin", PINOCCHIO_BINARY])
    if len(missing) > 0:
        print "Cant run test 2, missing dependencies: " + " ".join(missing)
    else:
        test_examples_must_finish_pin(examples)

    # Test 3
    missing = Shell.check_dependencies(programs, ["perf"])
    if len(missing) > 0:
        print "Cant run test 3, missing dependencies: " + " ".join(missing)
    else:
        test_examples_perf(examples)

    # Test 4
    missing = Shell.check_dependencies(programs, ["perf", "pin", PINOCCHIO_BINARY])
    if len(missing) > 0:
        print "Cant run test 4, missing dependencies: " + " ".join(missing)
    else:
        test_examples_perf_with_pin(examples)

    print_result(examples)
