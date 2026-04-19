#!/bin/bash
# ─────────────────────────────────────────────────────────────────────────────
# test_script.sh  –  Demo script to exercise BashDB GUI features
#
# Load this file in the GUI, set a breakpoint on the loop body,
# then press Run and step through the execution.
# ─────────────────────────────────────────────────────────────────────────────

# ── Simple variable assignment ────────────────────────────────────────────────
GREETING="Hello, bashdb!"
counter=0
MAX=5

echo "$GREETING"

# ── Function definition ────────────────────────────────────────────────────────
greet() {
    local name="$1"
    local msg="Debugging $name at step $counter"
    echo "$msg"
    return 0
}

# ── Loop with conditionals ─────────────────────────────────────────────────────
for i in $(seq 1 $MAX); do
    counter=$((counter + 1))

    if [ $((i % 2)) -eq 0 ]; then
        echo "Even: $i"
    else
        echo "Odd:  $i"
    fi

    greet "iteration-$i"
done

# ── Array demo ────────────────────────────────────────────────────────────────
fruits=("apple" "banana" "cherry" "date")
echo "Fruits: ${#fruits[@]} items"

for fruit in "${fruits[@]}"; do
    echo "  → $fruit"
done

# ── Arithmetic ────────────────────────────────────────────────────────────────
result=$((MAX * MAX))
echo "MAX² = $result"

# ── Read from stdin (tests console input forwarding) ─────────────────────────
# Uncomment the line below to test interactive input:
# read -p "Enter your name: " user_name && echo "Hello, $user_name!"

echo "Done. counter=$counter"
exit 0
