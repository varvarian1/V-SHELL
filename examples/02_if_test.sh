#!/usr/local/bin/v-shell

echo "=== Testing if/then/else ==="

if echo "Condition 1: true" then
    echo "Then branch executed"
fi

if false then
    echo "Then branch (should not execute)"
else
    echo "Else branch executed"
fi

echo "=== Testing nested if ==="

if echo "Outer condition" then
    echo "Outer then"
    if echo "Inner condition" then
        echo "Inner then"
    fi
fi

echo "=== Done ==="
