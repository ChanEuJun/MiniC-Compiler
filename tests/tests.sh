#!/bin/bash
set -e

# === ENVIRONMENT SETUP ===
export LLVM_INSTALL_PATH=/modules/cs325/llvm-21.1.0
export PATH=$LLVM_INSTALL_PATH/bin:$PATH
export LD_LIBRARY_PATH=$LLVM_INSTALL_PATH/lib:$LD_LIBRARY_PATH
CLANG=$LLVM_INSTALL_PATH/bin/clang++

if command -v module &> /dev/null; then
    module load GCC/13.3.0
fi

TEST_COMPILE_ONLY=0

while [[ $# -gt 0 ]];
do
  case $1 in
    -compile_only)
      TEST_COMPILE_ONLY=1
      shift 
      ;;
    *)
      echo "Unknown option: $1"
      exit 1
      ;;
  esac
done

DIR="$(pwd)"

# === BUILD COMPILER ===
echo "Cleanup *****"
rm -rf ./mccomp

echo "Compile Compiler *****"
make clean
make -j mccomp

COMP=$DIR/mccomp
echo "Compiler Binary: $COMP"

# === VALIDATION FUNCTIONS ===

function validate {
  $1 > perf_out
  echo
  echo "Executing: $1"
  grep "Result" perf_out
  grep "PASSED" perf_out
  rc=$?
  if [[ $rc != 0 ]]; then 
      echo "TEST FAILED *****"
      cat perf_out
      rm perf_out
      exit $rc
  fi
  rm perf_out
  # CLEANUP: Remove output.ll so it doesn't contaminate next test
  rm -f output.ll 
}

# Corrected Negative Validation
function validate_fail {
    local test_file=$1
    local test_name=$2
    
    echo "---------------------------------------------------"
    echo "Running Negative Test: $test_name"
    
    # 1. Ensure no stale output exists
    rm -f output.ll fail_out

    # 2. Run compiler. We strictly capture the exit code.
    # set +e allows command to fail without killing script
    set +e 
    $COMP $test_file > fail_out 2>&1
    comp_rc=$?
    set -e

    # 3. Logic:
    # If Exit Code == 0 (Success) AND output.ll exists -> FAIL (You accepted bad code)
    # If Exit Code != 0 (Error) -> PASS (You caught the error)
    
    if [ $comp_rc -eq 0 ] && [ -f "output.ll" ]; then
        echo "TEST FAILED: Compiler accepted invalid code in $test_name"
        echo "Compiler Output (unexpected success):"
        cat fail_out
        exit 1
    else
        # Verify "Error" is in the message for quality marks
        if grep -iq "Error" fail_out; then
            echo "PASSED: Compiler caught the error."
            grep -i "Error" fail_out | head -1
        else
            echo "WARNING: Compiler failed (good) but 'Error' keyword not found."
            cat fail_out
        fi
    fi
    rm -f fail_out output.ll
}

echo "Test *****"

# === TEST FLAGS ===
addition=1
factorial=1
fibonacci=1
pi=1
while=1
void=1
cosine=1
unary=1
palindrome=1
recurse=1
rfact=1
null=1
leap=1

# PART 3: ARRAY TESTS
array_addition=1
array_func_arg_1d=1
matrix_mul=1
global_array=1

# === STANDARD TESTS ===

if [ $addition == 1 ]; then	
	cd tests/addition/
	echo "[TEST] Addition"
	rm -rf output.ll add
	"$COMP" ./addition.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o add
        validate "./add"
    fi
    cd ../../
fi

if [ $factorial == 1 ]; then	
	cd tests/factorial
	echo "[TEST] Factorial"
	rm -rf output.ll fact
	"$COMP" ./factorial.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o fact
        validate "./fact"
    fi
    cd ../../
fi

if [ $fibonacci == 1 ]; then	
	cd tests/fibonacci
	echo "[TEST] Fibonacci"
	rm -rf output.ll fib
	"$COMP" ./fibonacci.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o fib
        validate "./fib"
    fi
    cd ../../
fi

if [ $pi == 1 ]; then	
	cd tests/pi
	echo "[TEST] PI"
	rm -rf output.ll pi
	"$COMP" ./pi.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o pi
        validate "./pi"
    fi
    cd ../../
fi

if [ $while == 1 ]; then	
	cd tests/while
	echo "[TEST] While Loop"
	rm -rf output.ll while
	"$COMP" ./while.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o while
        validate "./while"
    fi
    cd ../../
fi

if [ $void == 1 ]; then	
	cd tests/void
	echo "[TEST] Void Function"
	rm -rf output.ll void
	"$COMP" ./void.c 
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o void
        validate "./void"
    fi
    cd ../../
fi

if [ $cosine == 1 ]; then	
	cd tests/cosine
	echo "[TEST] Cosine"
	rm -rf output.ll cosine
	"$COMP" ./cosine.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o cosine
        validate "./cosine"
    fi
    cd ../../
fi

if [ $unary == 1 ]; then	
	cd tests/unary
	echo "[TEST] Unary Operators"
	rm -rf output.ll unary
	"$COMP" ./unary.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o unary
        validate "./unary"
    fi
    cd ../../
fi

if [ $recurse == 1 ]; then	
	cd tests/recurse
	echo "[TEST] Recursion"
	rm -rf output.ll recurse
	"$COMP" ./recurse.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o recurse
        validate "./recurse"
    fi
    cd ../../
fi

if [ $rfact == 1 ]; then	
	cd tests/rfact
	echo "[TEST] Recursive Factorial"
	rm -rf output.ll rfact
	"$COMP" ./rfact.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o rfact
        validate "./rfact"
    fi
    cd ../../
fi

if [ $palindrome == 1 ]; then	
	cd tests/palindrome
	echo "[TEST] Palindrome"
	rm -rf output.ll palindrome
	"$COMP" ./palindrome.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o palindrome
        validate "./palindrome"
    fi
    cd ../../
fi

if [ $leap == 1 ]; then
    cd tests/leap
    echo "[TEST] Leap Year"
    rm -rf output.ll leap
    "$COMP" ./leap.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG driver.cpp output.ll -o leap
        validate "./leap"
    fi
    cd ../../
fi

if [ $TEST_COMPILE_ONLY == 0 ]; then
    if [ $null == 1 ]; then	
        cd tests/null
        echo "[TEST] Null File"
        rm -rf output.ll null
        "$COMP" ./null.c 
        rc=$?
        if [[ $rc == 0 ]]; then
            echo "null test PASSED"
        else
            echo "null test FAILED"
        fi
        cd ../../
    fi
fi

# === ARRAY TESTS ===

if [ $array_addition == 1 ]; then	
    cd tests/array_addition
    echo "[TEST] Array Addition"
    rm -rf output.ll array_addition
    "$COMP" ./arr_addition.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG -g driver.cpp output.ll -o array_addition
        validate "./array_addition"
    fi
    cd ../../
fi

if [ $array_func_arg_1d == 1 ]; then	
    cd tests/array_func_arg_1d
    echo "[TEST] Array Function Argument"
    rm -rf output.ll array_func_arg
    "$COMP" ./arr_func_arg.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG -g driver.cpp output.ll -o array_func_arg
        validate "./array_func_arg"
    fi
    cd ../../
fi

if [ $matrix_mul == 1 ]; then	
    cd tests/matrix_multiplication
    echo "[TEST] Matrix Multiplication"
    rm -rf output.ll matrix_mul
    "$COMP" ./matrix_mul.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG -g driver.cpp output.ll -o matrix_mul
        validate "./matrix_mul"
    fi
    cd ../../
fi

if [ $global_array == 1 ]; then	
    cd tests/global_array
    echo "[TEST] Global Array"
    rm -rf output.ll global_array
    "$COMP" ./global_array.c
    if [ $TEST_COMPILE_ONLY == 0 ]; then
        $CLANG -g driver.cpp output.ll -o global_array
        validate "./global_array"
    fi
    cd ../../
fi

# === NEGATIVE TESTS (GENERATED) ===

echo ""
echo "***** RUNNING NEGATIVE TESTS (Error Reporting) *****"
mkdir -p tests/negative

# 1. Syntax Error (Missing Semicolon)
cat <<EOF > tests/negative/fail_syntax.c
int main() {
    int x;
    x = 5  // Missing semicolon
    return x;
}
EOF
validate_fail "tests/negative/fail_syntax.c" "Syntax Error (Missing Semicolon)"

# 2. Scope Error (Undeclared Variable)
cat <<EOF > tests/negative/fail_scope.c
int main() {
    x = 10; // x Undeclared
    return 0;
}
EOF
validate_fail "tests/negative/fail_scope.c" "Scope Error (Undeclared Variable)"

# 3. Type Narrowing Error (Float passed to Int)
cat <<EOF > tests/negative/fail_type.c
int foo(int a) {
    return a;
}
int main() {
    float f;
    f = 1.5;
    return foo(f); // Narrowing float->int
}
EOF
validate_fail "tests/negative/fail_type.c" "Type Error (Narrowing Conversion)"

# 4. Type Mismatch in Assignment
cat <<EOF > tests/negative/fail_assign.c
int main() {
    int arr[5];
    arr[0] = 1.5; // Narrowing float -> int
    return 0;
}
EOF
validate_fail "tests/negative/fail_assign.c" "Type Error (Assignment Narrowing)"

# Cleanup Negative Tests
rm -rf tests/negative

echo ""
echo "***** ALL TESTS PASSED *****"