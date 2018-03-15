#!/usr/bin/env bash

#------------------------------------------------------------------------------
# Bash script to run commandline Solidity to IELE tests.
#------------------------------------------------------------------------------

REPO_ROOT=$(cd $(dirname "$0")/.. && pwd)
FAILED="$REPO_ROOT"/test/failed
SOLC="$REPO_ROOT/build/solc/solc"

FULLARGS="--asm"

function printTask() { echo "$(tput bold)$(tput setaf 2)$1$(tput sgr0)"; }

function printResult()
{
    echo -e -n "$(tput bold)$(tput setaf 4)$1 passed: $(tput sgr0)"
    printf '%4d / %4d\n' $2 $3
}

function compileFull()
{
    local files="$*"
    local output failed

    output=$( ("$SOLC" $FULLARGS $files) 2>&1 )
    failed=$?

    if [ $failed -ne 0 ]
    then
        mkdir -p "$FAILED"
        failed_output="$FAILED/$(basename $1).failed"
        echo "Compilation failed on:" >> "$failed_output" 2>&1
        echo "$output" >> "$failed_output" 2>&1
        echo "While calling:" >> "$failed_output" 2>&1
        echo "\"$SOLC\" $FULLARGS $files" >> "$failed_output" 2>&1
        echo "Inside directory:" >> "$failed_output" 2>&1
        pwd >> "$failed_output" 2>&1
        for f in $files
        do
          echo >> "$failed_output" 2>&1
          echo "Source code of file $f" >> "$failed_output" 2>&1
          cat $f >> "$failed_output" 2>&1
        done
        false
    fi
}

printTask "Compiling all examples in std..."
examples=0
examples_success=0
cd "$REPO_ROOT"
for f in `ls std/*.sol | sort | comm -23 - test/failing-tests`
do
    compileFull "$f"
    failed=$?
    examples=$((examples+1))
    [ $failed -eq 0 ] && examples_success=$((examples_success+1))
    
done

printTask "Compiling various contracts and libraries..."
contracts=0
contracts_success=0
for dir in `ls -d test/compilationTests/* | sort | comm -23 - test/failing-tests`
do
    if [ "$dir" != "test/compilationTests/README.md" ]
    then
        compileFull "$dir"/*.sol "$dir"/*/*.sol
        failed=$?
        contracts=$((contracts+1))
        [ $failed -eq 0 ] && contracts_success=$((contracts_success+1))
    fi
done

printTask "Compiling all examples from the C++ test suite..."
ctests=0
ctests_success=0
TMPDIR=$(mktemp -d)
cd "$TMPDIR"
"$REPO_ROOT"/scripts/isolate_tests.py "$REPO_ROOT"/test/
for f in `ls *.sol | sort | comm -23 - "$REPO_ROOT"/test/failing-tests`
do
    compileFull "$f"
    failed=$?
    ctests=$((ctests+1))
    [ $failed -eq 0 ] && ctests_success=$((ctests_success+1))
done
cd ..
rm -rf "$TMPDIR"

printTask "Compiling all examples from the documentation..."
doctests=0
doctests_success=0
TMPDIR=$(mktemp -d)
cd "$TMPDIR"
"$REPO_ROOT"/scripts/isolate_tests.py "$REPO_ROOT"/docs/ docs
for f in `ls *.sol | sort | comm -23 - "$REPO_ROOT"/test/failing-tests`
do
    compileFull "$f"
    failed=$?
    doctests=$((doctests+1))
    [ $failed -eq 0 ] && doctests_success=$((doctests_success+1))
done
cd ..
rm -rf "$TMPDIR"

total=$((examples+contracts+ctests+doctests))
total_success=$((examples_success+contracts_success+ctests_success+doctests_success))
excluded=$(cat "$REPO_ROOT"/test/failing-tests | wc -l)
printResult "Std examples          " "$examples_success" "$examples"
printResult "Contracts             " "$contracts_success" "$contracts"
printResult "Test-suite examples   " "$ctests_success" "$ctests"
printResult "Documentation examples" "$doctests_success" "$doctests"
printResult "Total tests           " "$total_success" "$total"
printResult "Excluded              " 0 "$excluded" 

echo "Done."
[ "$total" -eq "$total_success" ]
