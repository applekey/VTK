#!/bin/bash
# check if oprofile and oreport is installed
hash operf 2>/dev/null || { echo >&2 "I require operf but it's not installed.  Aborting."; exit 1; }
hash opreport 2>/dev/null || { echo >&2 "I require opreport but it's not installed.  Aborting."; exit 1; }
 
 
TEST_TPROGRAM_PATH=/home/applekey/Documents/github/VTK/build/bin/HelloWorld
SPLIT_SMP_BLOCKS=500
NUMBER_OF_ITERATIONS=4
WORK_LOAD=5000
ENABLESMP="true"
THREAD_NUMBER=4
for i in {0..2..1}
do
     
     operf $TEST_TPROGRAM_PATH $THREAD_NUMBER $SPLIT_SMP_BLOCKS $NUMBER_OF_ITERATIONS $WORK_LOAD $ENABLESMP
     #opreport 
done
