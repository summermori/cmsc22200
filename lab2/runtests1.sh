# this script must be run on lab1/ otherwise it won't work

echo "" > tresults.txt

# converts all input files to hex
for f in ../lab1/inputs/*.s
do
  ./inputs/asm2hex $f
done

# runs the simulators for each file
for f in ../lab1/inputs/*.x
do
  echo "" > testsim.txt
  echo "" > testref.txt
  printf ">> Running ref on $f\n"
  echo "go" echo "rdum" echo "mdum 0x10000000 0x10000100" | ./sim $f >> testref.txt
  printf ">> Running sim on $f\n"
  echo "go" echo "rdum" echo "mdum 0x10000000 0x10000100" | ./src/sim $f >> testsim.txt
  echo ">> $f" >> tresults.txt
  diff -s testref.txt testsim.txt >> tresults.txt
  echo "" >> tresults.txt
done

