# this script must be run on lab1/ otherwise it won't work

# converts all input files to hex
for f in inputs/*.s
do
  ./inputs/asm2hex $f
done

# runs the simulators for each file
for f in inputs/*.x
do
  printf "\n\n>> Running ref on $f"
  ./sim $f
  printf "\n\n>> Running sim on $f"
  ./src/sim $f
  "\n>> Comparing dump files" >> testresults.txt
  diff -s dumpsim src/dumpsim >> testresults.txt
done
