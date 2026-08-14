#!/usr/local/bin/v-shell

if [ -f /etc/passwd ]; then
    if [ -r /etc/passwd ]; then
        echo "PASS: /etc/passwd is a readable file"
    else
        echo "FAIL: /etc/passwd is not readable"
    fi
else
    echo "FAIL: /etc/passwd not found"
fi
