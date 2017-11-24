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
    ''' all tests must finish under 100 seconds with different number
    of threads using PINocchio '''

    print header("Test: Examples must finish with pin")
    for ex in _examples:
        if ex.finishes:
            print ex.must_finish_pin(100)

def test_examples_must_finish_pin_with_time(_examples):
    ''' all tests must finish under 100 seconds with different number
    of threads using PINocchio with time option '''

    print header("Test: Examples must finish with pin using time option")
    for ex in _examples:
        if ex.finishes:
            print ex.must_finish_pin_with_time(100)

def test_examples_must_finish_pin_with_period(_examples):
    ''' all tests must finish under 100 seconds with different number
    of threads using PINocchio with period option '''

    print header("Test: Examples must finish with pin using period")
    for ex in _examples:
        if ex.finishes:
            print ex.must_finish_pin_with_period(100)

def print_result(_examples):
    ''' Print the result table, using information from all threads '''

    print header("Results")

    table = []
    for ex in _examples:
        table.append(ex.result())

    Example.print_table(Example.create_table_names(), table, 3)

def print_work_result(_examples):
    ''' Print the work result table, using information from all threads '''

    print header("Work Results")

    table = []
    for ex in _examples:
        table.append(ex.work_result())

    Example.print_table(Example.create_work_table_names(), table, 3)

def print_overhead_result(_examples):
    ''' Print the overhead result table, using information from all threads '''

    print header("Overhead Results")

    table = []
    for ex in _examples:
        table.append(ex.overhead_result())

    Example.print_table(Example.create_overhead_table_names(), table, 3)

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
    missing = Shell.check_dependencies(programs, ["pin", PINOCCHIO_BINARY])
    if len(missing) > 0:
        print "Cant run test 3, missing dependencies: " + " ".join(missing)
    else:
        test_examples_must_finish_pin_with_time(examples)

   # Test 4
    missing = Shell.check_dependencies(programs, ["pin", PINOCCHIO_BINARY])
    if len(missing) > 0:
        print "Cant run test 4, missing dependencies: " + " ".join(missing)
    else:
        test_examples_must_finish_pin_with_period(examples)

    # Print results collected
    print_result(examples)

    # FIXME: work and overhead is not working
    #print_work_result(examples)
    #print_overhead_result(examples)
