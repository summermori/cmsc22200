# this script must be run on lab1/ otherwise it won't work

echo "" > tresults.txt

# converts all input files to hex
for f in inputs/*.s
do
  ./inputs/asm2hex $f
done

# runs the simulators for each file
for f in inputs/*.x
do
  echo "" > testsim.txt
  echo "" > testref.txt
  printf ">> Running ref on $f\n"
  for i in {1..110};
  do
  	echo "run 1" echo "rdump"; done | ./src/ref_sim $f >> testref.txt
  printf ">> Running sim on $f\n"
  for i in {1..110};
  do
  	echo "run 1" echo "rdump"; done | ./src/sim $f >> testsim.txt
  echo ">> $f" >> tresults.txt
  diff -s testref.txt testsim.txt >> tresults.txt
  echo "" >> tresults.txt
done
