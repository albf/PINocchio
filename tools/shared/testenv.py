''' Simple helper classes for actual tests. Provides abstraction on
Shell and Example representation, allowing PINocchio to be called
from withing a python script'''

from distutils.spawn import find_executable
import os
import subprocess
import sys
import time

TOOLS_DIR = os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
PINOCCHIO_DIR = os.path.dirname(TOOLS_DIR)
PINOCCHIO_BINARY = os.path.join(PINOCCHIO_DIR, "obj-intel64", "PINocchio.so")

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
        p = subprocess.Popen(command.split())
        delay = 0.1

        passed = 0
        while p.poll() is None:
            time.sleep(delay)
            passed += delay
            if passed > timeout:
                p.kill()
                return None

        return p.returncode

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
        self.finishes = False
        self.finishes_with_pin = False

    def result(self):
        ''' generate string used as result table entry '''
        return [self.name, self.finishes, self.finishes_with_pin]

    def must_finish(self, timeout, threads=[1, 2, 4, 8, 16, 32]):
        ''' attempt to run the example using different thread numbers '''
        for t in threads:
            command = self.path + " " + str(t)
            r = Shell.execute(command, timeout)

            if r == None:
                return self.name + ": Failed/timeout to finish with " + str(t) + " threads"

            if r != 0:
                return self.name + ": Returned non-zero (" + str(r) + ") with " + str(r) + " threads"

        self.finishes = True
        return self.name + ": Ok"

    def must_finish_pin(self, timeout, threads=[1, 2, 4, 8, 16, 32]):
        ''' same as must_finish, but using pin and PINocchio '''
        for t in threads:
            command = "pin -t " + PINOCCHIO_BINARY + " -- " + self.path + " " + str(t)
            r = Shell.execute(command, timeout)

            if r == None:
                return self.name + ": Failed/timeout to finish with " + str(t) + " threads"

            if r != 0:
                return self.name + ": Returned non-zero (" + str(r) + ") with " + str(r) + " threads"

        self.finishes_with_pin = True
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
        return ["Name", "Finishes", "Finishes With Pin"]
