#!/bin/bash

### Usage: ./stepper <testfile> <number of steps>

# initializes out files
echo "" > simout.txt
echo "" > refout.txt
echo "" > compare.txt
echo "" > in.txt

yes "run 1 rdum" | head -n $2 >> in.txt

cat in.txt | ./sim $1 >> simout.txt
cat in.txt | ./ref_sim $1 >> refout.txt

diff -s refout.txt simout.txt >> compare.txt
