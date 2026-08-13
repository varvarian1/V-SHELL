#!/usr/local/bin/v-shell

until true; do
    echo "This should NOT be printed"
done
echo "Exited"
