#!/usr/bin/env bash

#------------------------------------------------------------------------------
# Bash script to run commandline Solidity to IELE tests.
#------------------------------------------------------------------------------

REPO_ROOT="${REPO_ROOT:-$(cd $(dirname "$0")/.. && pwd)}"
FAILED="$REPO_ROOT"/test/failed
SOLC="${SOLC:-$REPO_ROOT/build/solc/isolc}"

FULLARGS="--asm"

function printTask() { echo "$(tput bold)$(tput setaf 2)$1$(tput sgr0)"; }

function printResult()
{
    echo -e -n "$(tput bold)$(tput setaf 4)$1 passed: $(tput sgr0)"
    printf '%4d / %4d (%4d excluded)\n' $2 $3 $4
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

printTask "Compiling various contracts and libraries..."
contracts=0
contracts_success=0
contracts_excluded=0
for dir in test/compilationTests/*
do
    grep -q $dir test/should-skip-tests
    skipped=$?
    [ $skipped -eq 0 ] && continue
    if [ "$dir" != "test/compilationTests/README.md" ]
    then
        if `ls "$dir"/*/*.sol > /dev/null 2>&1`; then
            compileFull "$dir"/*.sol "$dir"/*/*.sol
            failed=$?
        else
            compileFull "$dir"/*.sol
            failed=$?
        fi
        grep -q $dir test/failing-tests
        excluded=$?
        contracts=$((contracts+1))
        [ $failed -eq 0 ] && [ $excluded -ne 0 ] && contracts_success=$((contracts_success+1))
        [ $failed -ne 0 ] && [ $excluded -eq 0 ] && contracts_excluded=$((contracts_excluded+1))
        [ $failed -eq 0 ] && [ $excluded -eq 0 ] && echo "Error: test passed but was expected to fail: $dir"
        [ $failed -ne 0 ] && [ $excluded -ne 0 ] && echo "Error: test failed but was expected to pass: $dir"
    fi
done

printTask "Compiling all examples from the C++ test suite..."
ctests=0
ctests_success=0
ctests_excluded=0
TMPDIR=$(mktemp -d)
cd "$TMPDIR"
"$REPO_ROOT"/scripts/isolate_tests.py "$REPO_ROOT"/test/
for f in *.sol
do
    grep -q $f "$REPO_ROOT"/test/should-skip-tests
    skipped=$?
    [ $skipped -eq 0 ] && continue
    compileFull "$f"
    failed=$?
    grep -q $f "$REPO_ROOT"/test/failing-tests
    excluded=$?
    ctests=$((ctests+1))
    [ $failed -eq 0 ] && [ $excluded -ne 0 ] && ctests_success=$((ctests_success+1))
    [ $failed -ne 0 ] && [ $excluded -eq 0 ] && ctests_excluded=$((ctests_excluded+1))
    [ $failed -eq 0 ] && [ $excluded -eq 0 ] && echo "Error: test passed but was expected to fail: $f" && cat "$f"
    [ $failed -ne 0 ] && [ $excluded -ne 0 ] && echo "Error: test failed but was expected to pass: $f" && cat "$f"
done
cd ..
rm -rf "$TMPDIR"

printTask "Compiling all examples from the documentation..."
doctests=0
doctests_success=0
doctests_excluded=0
TMPDIR=$(mktemp -d)
cd "$TMPDIR"
"$REPO_ROOT"/scripts/isolate_tests.py "$REPO_ROOT"/docs/ docs
for f in *.sol
do
    grep -q $f "$REPO_ROOT"/test/should-skip-tests
    skipped=$?
    [ $skipped -eq 0 ] && continue
    compileFull "$f"
    failed=$?
    grep -q $f "$REPO_ROOT"/test/failing-tests
    excluded=$?
    doctests=$((doctests+1))
    [ $failed -eq 0 ] && [ $excluded -ne 0 ] && doctests_success=$((doctests_success+1))
    [ $failed -ne 0 ] && [ $excluded -eq 0 ] && doctests_excluded=$((doctests_excluded+1))
    [ $failed -eq 0 ] && [ $excluded -eq 0 ] && echo "Error: test passed but was expected to fail: $f" && cat "$f"
    [ $failed -ne 0 ] && [ $excluded -ne 0 ] && echo "Error: test failed but was expected to pass: $f" && cat "$f"
done
cd ..
rm -rf "$TMPDIR"

total=$((examples+contracts+ctests+doctests))
total_success=$((examples_success+contracts_success+ctests_success+doctests_success))
total_excluded=$((examples_excluded+contracts_excluded+ctests_excluded+doctests_excluded))
printResult "Std examples          " "$examples_success" "$examples" "$examples_excluded"
printResult "Contracts             " "$contracts_success" "$contracts" "$contracts_excluded"
printResult "Test-suite examples   " "$ctests_success" "$ctests" "$ctests_excluded"
printResult "Documentation examples" "$doctests_success" "$doctests" "$doctests_excluded"
printResult "Total tests           " "$total_success" "$total" "$total_excluded"

echo "Done."
[ "$total" -eq $(($total_success+$total_excluded)) ]
