import matplotlib.pyplot as plt
import json

def process_thread(thread):
    # Order: unlocked, locked, unregistered, finished
    colors_map = ["b", "r", "k", "w"]

    left = []
    duration = []
    color = []

    size = 0
    offset = thread["start"]

    if (len(thread["samples"]) > 0):
        # Unregistered and omitted part
        if (offset > 0):
            left.append(0)
            duration.append(offset)
            color.append(colors_map[2])
            size += 1

        accumulated = 0
        current = thread["samples"][0]

        for e in thread["samples"]:
            # add only when status changes
            if (current != e):
                left.append(offset)
                duration.append(accumulated)
                color.append(colors_map[int(current)])

                offset += accumulated
                size += 1
                current = e
                accumulated = 1
            else:
                accumulated += 1

        # add last one
        size += 1
        left.append(offset)
        duration.append(accumulated)
        color.append(colors_map[int(current)])

    return (left, duration, color, size)

def calculate_work(duration, color, color_selected):
    work = 0
    for i in range(len(duration)):
        if color_selected == color[i]:
            work += duration[i]
    return work



if __name__ == "__main__":

    with open('trace.json') as data_file:
        data = json.load(data_file)

    print "json parsed correctly, processing..."
    sample_size = data["sample-size"]
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
        t_left, t_duration, t_color, t_size = process_thread(threads[t])

        # update bars information
        left += t_left
        duration += t_duration
        color += t_color

        # update Y title information
        ypos += (t_size*[t])
        actors.append("thread " + str(t))

        # update Final execution statistics
        work += sample_size*calculate_work(t_duration, t_color, "b")

    print "------"
    print "Total Work: " + str(work) + " cycles"
    print "------"

    print "processing done, plotting graph"
    plt.barh(ypos, duration, left=left, align='center', alpha=0.4, color=color)
    plt.xlabel('Cycles (*' + str(sample_size) + ")")
    plt.yticks(range(len(threads)), actors)
    plt.title('Threads Events')
    plt.show()
