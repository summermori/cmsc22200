echo "" > tresults.txt

# converts all input files to hex
for f in inputs/*.s
do
  ../lab3/inputs/asm2hex $f
done

for g in ../lab3/inputs/*-lab4.s
do
  cp $g inputs
done

# runs the simulators for each file
for f in inputs/*.x
do
  echo "" > testsim.txt
  echo "" > testref.txt
  printf ">> Running ref on $f\n"
  for i in {1..110};
  do
  	echo "run 1" echo "rdump"; done | ./src/refsim $f >> testref.txt
  printf ">> Running sim on $f\n"
  for i in {1..110};
  do
  	echo "run 1" echo "rdump"; done | ./src/sim $f >> testsim.txt
  echo ">> $f" >> tresults.txt
  diff -s testref.txt testsim.txt >> tresults.txt
  echo "" >> tresults.txt
done
