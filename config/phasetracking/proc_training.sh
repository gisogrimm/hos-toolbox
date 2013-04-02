#!/bin/bash
for f in pattern_??.wav; do
    sox $f $f temp.wav
    FILE=temp
    export FILE
    mha ?read:phasetrack.cfg
    mv temp_feat.mat ${f/.wav/_feat.mat}
done
#FILE=pattern mha ?read:phasetrack.cfg
#FILE=patterntest mha ?read:phasetrack.cfg
