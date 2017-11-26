''' Simple helper classes for actual tests. Provides abstraction on
Shell and Example representation, allowing PINocchio to be called
from withing a python script'''

from distutils.spawn import find_executable
from shared.trace import all_work_from_file
import os
import subprocess
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
PINOCCHIO_DIR = os.path.dirname(TOOLS_DIR)
PINOCCHIO_BINARY = os.path.join(PINOCCHIO_DIR, "obj-intel64", "PINocchio.so")

NUM_TESTS = 6
VERBOSE = True

class Shell(object):
    ''' Extra helpful layer for shell related functions '''
    @staticmethod
    def search_programs():
        ''' search tools of interest on path '''
        search_list = ["pin", "perf", PINOCCHIO_BINARY]
        programs = {}

        for p in search_list:
            if find_executable(p):
                programs[p] = True
            else:
                print "Warning: Couldn't find " + p
                programs[p] = False

        return programs

    @staticmethod
    def execute(command, timeout):
        ''' execute a given command but kill if it doesn't finish in time '''
        if VERBOSE:
            print "  $ " + command

        p = subprocess.Popen(command.split(), cwd=TOOLS_DIR, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        delay = 0.1

        passed = 0
        while p.poll() is None:
            time.sleep(delay)
            passed += delay
            if passed > timeout:
                p.kill()
                return None, None, None

        out, err = p.communicate()
        return p.returncode, out, err

    @staticmethod
    def create_examples():
        ''' identify current examples and create examples objects for them '''
        ls = os.listdir(os.path.join(PINOCCHIO_DIR, "examples"))
        binary_dir = os.path.join(PINOCCHIO_DIR, "obj-intel64")

        examples = []
        for f in ls:
            if f.endswith("_app.c"):
                test_name = f[:-2]
                test_file = os.path.join(binary_dir, test_name)
                if os.path.isfile(test_file):
                    examples.append(Example(test_name, test_file))
                else:
                    print "Warning: Test missing: " + test_file

        return examples

    @staticmethod
    def check_dependencies(programs, dependencies):
        ''' check for missing dependencies using program map '''
        missing = []

        for d in dependencies:
            if d not in programs:
                print "Error: Missing " + d + " as programs entry"
                missing.append(d)
                continue

            if not programs[d]:
                missing.append(d)
                continue

        return missing



class Example(object):
    ''' Represents a example, which should have a *_app.c file under dir
    /examples/ and a compiled binary under obj-intel64/ '''
    def __init__(self, name, path):
        self.name = name
        self.path = path
        self.threads = Example.quadratic_range(NUM_TESTS)

        self.finishes = False
        self.finishes_with_pin = False
        self.finishes_with_time = False
        self.finishes_with_period = False
        self.finishes_with_pin_and_perf = False

        # List of results following the order of self.threads
        self.work = []

        # Format: [[wall, cpu]]
        self.result = []
        self.result_with_pin = []
        self.result_with_time = []
        self.result_with_period = []

    def finish_result(self):
        ''' generate string used as result table entry '''
        return [self.name, self.finishes, self.finishes_with_pin, self.finishes_with_time, self.finishes_with_period]

    def work_result(self):
        ''' generate string used as work result table entry '''
        result = [self.name]
        for i in range(NUM_TESTS):
            if self.finishes_with_pin_and_perf:
                result.append(self.perf_with_pin_stats[i][1]/float(self.work[i]))
            else:
                result.append(None)
        for i in range(NUM_TESTS):
            if self.finishes_with_pin_and_perf:
                result.append(self.perf_with_pin_stats[i][0]/float(self.work[i]))
            else:
                result.append(None)
        return result

    def append_stdout_results(self, stdout, result_list):
        ''' find results in stdout and append to the provided list'''
        wall, cpu = None, None
        lines = stdout.splitlines()

        for l in lines:
            if l.startswith("@WALL:"):
                wall = float(l[7:])
            if l.startswith("@CPU:"):
                cpu= float(l[6:])

        if wall is None:
            print "Warning: Couldn't find wall time on stdout"
        if cpu is None:
            print "Warning: Couldn't find cpu time on stdout"
        result_list.append([wall, cpu])

    def format_float(self, value):
        return "{:10.2f}".format(value).strip()

    def overhead_pin(self):
        ''' generate result table entry for regular usage'''
        overhead = ["regular"]

        # If not even finishes, just stop.
        if not self.finishes:
            return overhead

        for i in range(NUM_TESTS):
            if self.finishes_with_pin:
                overhead.append(self.format_float(self.result_with_pin[i][0]/float(self.result[i][0])))
                overhead.append(self.format_float(self.result_with_pin[i][1]/float(self.result[i][1])))
            else:
                overhead.append(None)
                overhead.append(None)

        return overhead

    def overhead_time(self):
        ''' generate result table entry for time usage'''
        overhead = ["time"]

        # If not even finishes, just stop.
        if not self.finishes:
            return overhead

        for i in range(NUM_TESTS):
            if self.finishes_with_time:
                overhead.append(self.format_float(self.result_with_time[i][0]/float(self.result[i][0])))
                overhead.append(self.format_float(self.result_with_time[i][1]/float(self.result[i][1])))
            else:
                overhead.append(None)
                overhead.append(None)

        return overhead

    def overhead_period(self):
        ''' generate result table entry for period usage'''
        overhead = ["period"]

        # If not even finishes, just stop.
        if not self.finishes:
            return overhead

        for i in range(NUM_TESTS):
            if self.finishes_with_period:
                overhead.append(self.format_float(self.result_with_period[i][0]/float(self.result[i][0])))
                overhead.append(self.format_float(self.result_with_period[i][1]/float(self.result[i][1])))
            else:
                overhead.append(None)
                overhead.append(None)

        return overhead

    def _threads_str(self, t):
        ''' Just output the right word, plural or singular'''
        if t > 1:
            return " threads"
        return " thread"

    def must_finish(self, timeout):
        ''' attempt to run the example using different thread numbers '''
        for t in self.threads:
            command = self.path + " " + str(t)
            r, stdout, _ = Shell.execute(command, timeout)

            if r == None:
                return self.name + ": Failed/timeout to finish with " + str(t) + self._threads_str(t)

            if r != 0:
                return self.name + ": Returned non-zero (" + str(r) + ") with " + str(t) + self._threads_str(t)

            self.append_stdout_results(stdout, self.result)

        self.finishes = True
        return self.name + ": Ok"

    def must_finish_pin(self, timeout):
        ''' same as must_finish, but using pin and PINocchio, also calculates work '''
        for t in self.threads:
            command = "pin -t " + PINOCCHIO_BINARY + " -- " + self.path + " " + str(t)
            r, stdout, _ = Shell.execute(command, timeout)

            if r == None:
                return self.name + ": Failed/timeout to finish with " + str(t) + self._threads_str(t)

            if r != 0:
                return self.name + ": Returned non-zero (" + str(r) + ") with " + str(t) + self._threads_str(t)

            self.append_stdout_results(stdout, self.result_with_pin)

        self.finishes_with_pin = True
        return self.name + ": Ok"

    def must_finish_pin_with_time(self, timeout):
        ''' same as must_finish, but using pin and PINocchio with timed based (non-PRAM) option'''
        for t in self.threads:
            command = " pin -t " + PINOCCHIO_BINARY + " -t -- "
            command += self.path + " " + str(t)
            r, stdout, _ = Shell.execute(command, timeout)

            if r == None:
                return self.name + ": Failed/timeout to finish with " + str(t) + self._threads_str(t)

            if r != 0:
                return self.name + ": Returned non-zero (" + str(r) + ") with " + str(t) + self._threads_str(t)

            self.append_stdout_results(stdout, self.result_with_time)

        self.finishes_with_time = True
        return self.name + ": Ok"

    def must_finish_pin_with_period(self, timeout):
        ''' same as must_finish, but using pin and PINocchio with a 1000-period (approximate) option'''
        for t in self.threads:
            command = " pin -t " + PINOCCHIO_BINARY + " -p 1000 -- "
            command += self.path + " " + str(t)
            r, stdout, _ = Shell.execute(command, timeout)

            if r == None:
                return self.name + ": Failed/timeout to finish with " + str(t) + self._threads_str(t)

            if r != 0:
                return self.name + ": Returned non-zero (" + str(r) + ") with " + str(t) + self._threads_str(t)

            self.append_stdout_results(stdout, self.result_with_period)

        self.finishes_with_period = True
        return self.name + ": Ok"

    @staticmethod
    def print_table(names, table, spacing):
        ''' generic print table function, used to print results '''
        maxsize = []

        for title in names:
            maxsize.append(len(title)+spacing)

        for i in range(len(names)):
            for line in table:
                aux = (len(str(line[i]))+spacing)
                if aux > maxsize[i]:
                    maxsize[i] = aux

        for index in range(len(names)):
            sys.stdout.write(str(names[index]).ljust(maxsize[index]))
        for line in table:
            sys.stdout.write('\n')
            for index in range(len(names)):
                sys.stdout.write(str(line[index]).ljust(maxsize[index]))
        sys.stdout.write('\n')

    @staticmethod
    def create_table_names():
        ''' current result values, what values it generates '''
        return ["Name", "Normal", "Pin", "Time", "Period"]

    @staticmethod
    def quadratic_range(total):
        ''' Generates a range that increases quadratically '''
        entries = range(total)
        quad = [2**x for x in entries]
        return quad

    @staticmethod
    def create_work_table_names():
        ''' current work values, what values it generates '''
        insWork = []
        cycWork = []
        for i in Example.quadratic_range(NUM_TESTS):
            insWork.append("Ins/Work (" + str(i) + ")")
            cycWork.append("Cyc/Work (" + str(i) + ")")

        return ["Name"] + insWork + cycWork

    @staticmethod
    def create_overhead_table_names():
        ''' current overhead values, what values it generates '''
        values = []
        for i in Example.quadratic_range(NUM_TESTS):
            values.append("Wall-" + str(i))
            values.append("Cpu-" + str(i))

        return ["Type/Time"] + values
