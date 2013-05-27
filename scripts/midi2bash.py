#!/usr/bin/python

# load bash script "loadpreset" on program change messages.

from mididings import *

def ev2bash(ev):
    return "./loadpreset {0:d}".format(ev.program)

run(
    Filter(PROGRAM) >> System(ev2bash),
)
