import csv
import numpy as np
import matplotlib.pyplot as plt
import math
import sys
import json
from pprint import pprint
import matplotlib.gridspec as gridspec
import matplotlib.patches as mpatches

def getthr(thstp, thstr, endp):
    v = []
    if thstp == -1:
        return []

    for i in range(endp):
        if i > thstp:
            if thstr[i-thstp] == '1':
                v.append(1)
            else:
                v.append(0)
    return v

with open('trace.json') as data_file:
    data = json.load(data_file)

print "Drawing the graphic..."
logend = data["END"]
allthdata = [] # Each thread: [ START, [STR]]

thstp = data["MAIN"]["STP"]
allthdata.append( [ thstp , getthr(thstp, data["MAIN"]["STR"], logend), 0] )
for th in data["THREADS"]:
    thstp = th["STP"]
    allthdata.append( [thstp, getthr(thstp, th["STR"], logend), 0] )

threadchecked = 0  # For debug
for th in allthdata:
    if th[0] != -1:
        threadchecked += 1
    else:
        break

allthdata = allthdata[:threadchecked]

szofoff = []
szofon  = []
szofnull= []

#checking if NULL
for i in range(len(allthdata)):
    szofnull.append(allthdata[i][0])
    allthdata[i][2] = 0

for z in range(logend):
    exit_status = True
    for i in range(len(allthdata)):
        if allthdata[i][2] < (logend):
            exit_status = False
    if exit_status == True:
        break

    #checking if off
    v = []
    for i in range(len(allthdata)):
        counter = 0
        for j in range(allthdata[i][2], len(allthdata[i][1])):
            if allthdata[i][1][j] == 0:
                counter += 1
                allthdata[i][2] += 1
            else:
                break
        v.append(counter)
    szofoff.append(v)
   # print "V(off) = ", v

    #checking if off
    v = []
    for i in range(len(allthdata)):
        counter = 0
        for j in range(allthdata[i][2], len(allthdata[i][1])):
            if allthdata[i][1][j] == 1:
                counter += 1
                allthdata[i][2] += 1
            else:
                break
        v.append(counter)
    szofon.append(v)
  #  print "V(on ) = ", v

#equalize sizes
zero = [0 for i in range(len(allthdata))]
if len(szofoff) > len(szofon):
    while len(szofon) != len(szofoff):
        szofon.append(zero)
else:
    while len(szofoff) != len(szofon):
        szofoff.append(zero)

plt.subplot(111)
accum = [0 for i in range(len(allthdata))]
thindex = [i for i in range(len(allthdata))]
plt.barh(thindex,szofnull,left=accum, hold=True, color='k', linewidth=0, align='center')

for i in range(len(allthdata)):
    accum[i] += szofnull[i]

for r in range(len(szofon)):
    plt.barh(thindex,szofon[r],left=accum,hold=True,color='b',linewidth=0, align='center')
    for i in range(len(allthdata)):
        accum[i] += szofon[r][i]
    plt.barh(thindex,szofoff[r],left=accum,hold=True, color='r', linewidth=0,  align='center')
    for i in range(len(allthdata)):
        accum[i] += szofoff[r][i]


tname = ["Thread "+str(i) for i in range(threadchecked-1)]
tname = ["MAIN"] + tname
plt.yticks(thindex, tname)
plt.xticks([i*(logend)/5 for i in range(5)], [i*20 for i in range(5)])
plt.xlim(xmax=logend)

red_patch   = mpatches.Patch(color='red', label='Locked')
blue_patch  = mpatches.Patch(color='blue', label='Unlocked')
black_patch = mpatches.Patch(color='black', label='Unstarted')
plt.legend(handles=[red_patch, blue_patch, black_patch], loc=2)
plt.xlabel("% Execution")
plt.title("PINocchio Report")
#plt.show()
plt.savefig('graph.png')
