# function to create input file
function create_input {
  yes run 1 rdum mdum 0x10000000 0x10000024 | head -n $1 > input.txt
}

# function to step through both sim and ref
function run_simulators {
  cat input.txt | ./sim $1 > simout.txt
  cat input.txt | ./ref_sim $1 > refout.txt
}

# function to run all tests
function run_all_tests {
  echo "" > results.txt
  for f in ../inputs/*.x
  do
    run_simulators $f
    echo "TESTING FILE $f"
    echo "" >> results.txt
    echo "##########  TESTING FILE $f  ##########" >> results.txt
    diff -s simout.txt refout.txt >> results.txt
  done
}

# loop to put it all together
while getopts ":c:af:" opt; do
  case ${opt} in
    c )
      create_input $OPTARG
      ;;
    a )
      run_all_tests
      exit 0
      ;;
    f )
      run_simulators $OPTARG
      diff -s simout.txt refout.txt > results.txt
      exit 0
      ;;
    \? )
      echo "Invalid option: $OPTARG" 1>&2
      exit 1
      ;;
    : )
      echo "Invalid option: $OPTARG requires an argument" 1>&2
      exit 1
      ;;
  esac
done
shift $((OPTIND -1))
