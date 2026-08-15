#!/usr/local/bin/v-shell

echo "=== Test condition with && ==="
if [ -f /etc/passwd ] && [ -r /etc/passwd ]; then
    echo "PASS: file exists and readable"
else
    echo "FAIL"
fi

echo "=== Test condition with || ==="
if [ -f /nonexistent ] || [ -f /etc/passwd ]; then
    echo "PASS: at least one file exists"
else
    echo "FAIL"
fi

echo "=== Test condition with ; (should fail) ==="
if [ -f /etc/passwd ]; then
    echo "PASS: simple if"
fi