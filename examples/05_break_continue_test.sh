#!/usr/local/bin/v-shell

for i in 1 2 3; do
    if true; then continue; fi
    echo "not printed"
done
echo "after loop"