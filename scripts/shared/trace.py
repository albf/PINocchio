''' trace.py
Copyright (C) 2017 Alexandre Luiz Brisighello Filho

This software may be modified and distributed under the terms
of the MIT license.  See the LICENSE file for details.

Auxiliary functions regarding trace processing
'''

import json

def process_thread(thread):
    ''' Generate information regarding one thread: left positions,
    duration of each sample, it color and how many were added '''

    # Order: unlocked, locked, unregistered, finished
    colors_map = ["b", "r", "k", "w"]

    _left = []
    _duration = []
    _color = []

    size = 0
    offset = thread["start"]

    if len(thread["samples"]) > 0:
        # Unregistered and omitted part
        if offset > 0:
            _left.append(0)
            _duration.append(offset)
            _color.append(colors_map[2])
            size += 1

        for e in range(len(thread["samples"])-1):
            # Duration is the difference between two changes
            duration = thread["samples"][e+1][0] - thread["samples"][e][0]
            current = thread["samples"][e][1]

            _left.append(offset)
            _duration.append(duration)
            _color.append(colors_map[int(current)])

            offset += duration
            size += 1

    return (_left, _duration, _color, size)

def thread_work(_duration, _color, color_selected):
    ''' Return work in instructions for a given thread and color '''
    _work = 0
    for i in range(len(_duration)):
        if color_selected == _color[i]:
            _work += _duration[i]
    return _work

def all_stats(_threads):
    ''' Return all work in instructions for all threads '''
    _work = 0
    for _t in _threads:
        _, d, c, _ = process_thread(_t)
        _work += thread_work(d, c, "b")

    _max_duration = duration(_threads)
    _efficiency = _work/float(_max_duration*len(_threads))

    return _work, _max_duration, _efficiency

def all_stats_from_file(filename):
    ''' All work but from a trace file '''
    with open(filename) as data_file:
        data = json.load(data_file)

    return all_stats(data["threads"])

def duration(_threads):
    ''' Duration of computation, from the start of first thread and
    end of last thread to exit '''

    # Currently, thread 0 must be responsible for starting and
    # finishing threads, so it's both first and last.
    loop = len(_threads[0]["samples"])-1
    return _threads[0]["samples"][loop][0]

def validate(_data):
    ''' Common validations on a corrected parsed json '''

    _threads = _data["threads"]
    i = -1
    for _t in _threads:
        i = i + 1
        if _t["start"] < 0:
            print "Warning, thread with negative start"

        p_counter = _t["start"]
        p_state = -1
        for _d in _t["samples"]:
            if p_counter > _d[0]:
                print "Warning: Bad sample counters order for " + str(i) + " on: " + str(p_counter)
            if p_state == _d[1]:
                print "Warning: Bad sample states order for " + str(i) + "on: " + str(p_counter)

            p_counter = _d[0]
            p_state = _d[1]
