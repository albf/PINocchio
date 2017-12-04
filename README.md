# PINocchio

PRAM (parallel random-access machine) simulator for pthreads-based programs, forcing the system to fairly execute the same number of instructions for each current unlocked thread.

## Getting Started

PINocchio is a pintool, so you need a C++ toolchain and Intel's Pin to compile the project. Although Pin is compatible with Windows, PINocchio was tested and developed with Linux in mind, as it tracks pthreads calls - check examples.

## Installing

Most tests were performed on Debian, but it should work the same way on other distros assuming you respect the rules and can provide the following:

- gcc and g++ (tested with 6.3.0)
- make
- pin (tested with 3-4, get it [here](https://software.intel.com/en-us/articles/pin-a-binary-instrumentation-tool-downloads))
- [Optional] python 2.7 with matplotlib

Python is only required for using the scripts provided. Scripts [graph.py](scripts/graph.py) and [scale.py](scripts/scale.py) are actually the easiest way to visualize the results from an execution, [benchmark.py](scripts/benchmark.py) is for testing purposes.

Once you have the requirements, installing follows as other pintools:

- extract pin tarball
- clone this repo into pin-3.4/source/tools/
- inside pin-3.4/source/tools/, "make" it

Lastly, before running anything, remind you need to have Pin binary in your PATH variable.

## Usage

Something to keep in mind, all sync is done by the tool, so the pthreads functions used should be supported. Here is the current supported list:

- creation
    - pthread_create
    - pthread_join
    - pthread_exit (or just return)
- mutex
    - pthread_mutex_init
    - pthread_mutex_destroy
    - pthread_mutex_lock
    - pthread_mutex_trylock
    - pthread_mutex_unlock
- semaphore
    - sem_init
    - sem_destroy
    - sem_getvalue
    - sem_post
    - sem_trywait
    - sem_wait
- rwlock
    - pthread_rwlock_init
    - pthread_rwlock_destroy
    - pthread_rwlock_rdlock
    - pthread_rwlock_tryrdlock
    - pthread_rwlock_wrlock
    - pthread_rwlock_trywrlock
    - pthread_rwlock_unlock
- cond
    - pthread_cond_init
    - pthread_cond_destroy
    - pthread_cond_broadcast
    - pthread_cond_signal
    - pthread_cond_wait

Assuming you have installed correctly, you should have PINocchio.so inside obj-intel64/ subdirectory. To make it easier to use, a bash script is provided. For the pi_montecarlo_app, for example, the normal usage would be:

```
$ ./PINocchio.sh ./obj-intel64/pi_montecarlo_app
```

Once the execution has finished, a JSON with the generated results (trace.json) is created once execution is finished. You can visualize the execution by using the provided scripts. To explore only one trace:

```
$ python scripts/graph.py
```

There are two extra execution modes you use, by adding an argument just in front the `PINocchio.so`:

- -p NUMBER
    - would sync only after a period of NUMBER cycles. Can be used to get a less precise result but faster.
    - example: $ ./PINocchio.sh -p 1000 ./obj-intel64/pi_montecarlo_app
- -t
    - time based simulation without sync, not a PRAM. Can be used for comparison or only tracking threads.
    - example: $ ./PINocchio.sh -t ./obj-intel64/pi_montecarlo_app
- -o NAME
    - just change the output name.
    - example: $ ./PINocchio.sh -o other.json ./obj-intel64/pi_montecarlo_app


For all the examples, the first argument is the number of threads to be created. Here follows the pi_montecarlo_app executed with 4 worker threads and the generated graph.

```
$ ./PINocchio.sh ./obj-intel64/pi_montecarlo_app 4
$ python scripts/graph.py
```

![Pi generated output](/imgs/graph-4/pi_montecarlo_app-4.png)

### Scale

There is also a scale script. It will run an example several times, changing the number of threads on each execution (it assumes the software receives the number of threads as the first argument). After all the executions, it will calculate and plot: total work, duration and efficiency. Using "-p" will use a 1000-period.

```
$ python scripts/scale.py -p ./obj-intel64/pi_montecarlo_app
```

![Pi scale generated output](/imgs/scale/pi_montecarlo.png)

Others graphs can be found at the [imgs](/imgs) directory.


## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details
