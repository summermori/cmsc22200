# this script must be run on lab1/ otherwise it won't work

# converts all input files to hex
for f in inputs/*.s
do
  ./inputs/asm2hex $f
done

# runs the simulators for each file
for f in inputs/*.x
do
  echo ">> Running ref on $f"
  ./sim $f
  echo ">> Running sim on $f"
  ./src/sim $f
  echo ">> Comparing dump files"
  diff -s dumpsim src/dumpsim
done
