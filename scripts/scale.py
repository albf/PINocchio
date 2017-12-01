''' scale.py
Copyright (C) 2017 Alexandre Luiz Brisighello Filho

This software may be modified and distributed under the terms
of the MIT license.  See the LICENSE file for details.

Performs scalability tests for an example, plotting how it would
scale in terms of work|duration|efficiency versus number of threads.
'''

from shared.testenv import Example, Shell, PINOCCHIO_BINARY
import matplotlib.pyplot as plt
import sys

def test_examples_must_finish(_examples):
    ''' all tests must finish under 5 seconds with different number
    of threads '''

    for ex in _examples:
        print ex.must_finish(5)

def plot_result(_example):
    x = _example.threads
    x_label = 'Number of Threads'
    y_work = _example.work
    y_duration = _example.max_duration
    y_efficiency = _example.efficiency

    plt.subplot(3, 1, 1)
    plt.plot(x, y_work, 'o-')
    plt.xlabel(x_label)
    plt.ylabel('Total Work (Cycles)')

    plt.subplot(3, 1, 2)
    plt.plot(x, y_duration, 'o-')
    plt.xlabel(x_label)
    plt.ylabel('Total Duration (Cycles)')

    plt.subplot(3, 1, 3)
    plt.plot(x, y_efficiency, 'o-')
    plt.xlabel(x_label)
    plt.ylabel('Efficiency')

    plt.show()

if __name__ == "__main__":
    # If an argument, it's the filename
    if (len(sys.argv) < 2):
        print "Missing program name."
        exit(1)

    period = False
    instrumented = sys.argv[-1]

    # If there is any extra argument before it, assume period
    if (len(sys.argv) > 2):
        period = True

    programs = Shell.search_programs()
    example = Shell.create_example(instrumented, no_binary_fail = True, quadratic = False)

    # Assert test run before loosing time.
    test_examples_must_finish([example])

    missing = Shell.check_dependencies(programs, ["pin", PINOCCHIO_BINARY])
    if len(missing) > 0:
        exit(1)

    if period:
        example.scale_test_pin_with_period(100)
    else:
        example.scale_test_pin(500)

    plot_result(example)
