''' Reposponsible for generating a matplotlib plot of a trace json
generated by PINocchio '''

import matplotlib.pyplot as plt
import json
from shared import trace


if __name__ == "__main__":

    with open('trace.json') as data_file:
        data = json.load(data_file)

    trace.validate(data)
    print "json parsed correctly, processing..."
    end = data["end"]
    threads = data["threads"]

    # Bars information
    left = []
    duration = []
    color = []

    # Y title information
    ypos = []
    actors = []

    # Final execution statistics
    work = 0

    for t in range(len(threads)):
        t_left, t_duration, t_color, t_size = trace.process_thread(threads[t])

        # update bars information
        left += t_left
        duration += t_duration
        color += t_color

        # update Y title information
        ypos += (t_size*[t])
        actors.append("thread " + str(t))

        # update Final execution statistics
        work += trace.thread_work(t_duration, t_color, "b")

    max_duration = trace.duration(threads)
    efficiency = work/float(max_duration*len(threads))

    print "------"
    print "Total Work: " + str(work) + " cycles"
    print "Duration:   " + str(max_duration) + " cycles"
    print "Efficiency: " + str(efficiency)
    print "------"

    print "processing done, plotting graph"
    plt.barh(ypos, duration, left=left, align='center', alpha=0.4, color=color)
    plt.xlabel('Cycles')
    plt.yticks(range(len(threads)), actors)
    plt.title('Threads Events')
    plt.show()
