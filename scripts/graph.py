''' graph.py
Copyright (C) 2017 Alexandre Luiz Brisighello Filho

This software may be modified and distributed under the terms
of the MIT license.  See the LICENSE file for details.

Reposponsible for generating a matplotlib plot of a trace json
generated by PINocchio
'''

import matplotlib.pyplot as plt
import json
import sys
from shared import trace


if __name__ == "__main__":
    filename = 'trace.json'

    # If an argument, it's the filename
    if (len(sys.argv) > 1):
        filename = sys.argv[1]

    with open(filename) as data_file:
        data = json.load(data_file)

    trace.validate(data)
    print "json parsed correctly, processing..."
    unit = data["unit"]
    end = data["end"]
    threads = data["threads"]

    # Bars information
    left = []
    duration = []
    color = []

    # Y title information
    ypos = []
    actors = []

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
    work, max_duration, efficiency = trace.all_stats(data["threads"])

    print "------"
    print "Total Work: " + str(work) + " " + unit
    print "Duration:   " + str(max_duration) + " " + unit
    print "Efficiency: " + str(efficiency)
    print "------"

    print "processing done, plotting graph"
    plt.barh(ypos, duration, left=left, align='center', alpha=0.4, color=color)
    plt.xlabel(unit)
    plt.yticks(range(len(threads)), actors)
    plt.title('Threads Events')
    plt.show()
