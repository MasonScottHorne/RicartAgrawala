gcc -o print print.c

gcc -o node node.c
./node 1 &
./node 2 &
./node 3 &
./node 4 &

gcc -o hacker hacker.c
