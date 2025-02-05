# Ricart-Agrawala Distributed Mutual Exclusion Algorithm
Ricart, G., & Agrawala, A. K. (1981). An Optimal Algorithm for Mutual Exclusion in Computer Networks.
https://dl.acm.org/doi/pdf/10.1145/358527.358537
## Compilation
```sh
gcc -o print print.c
./print

gcc -o node node.c
./node 1 &
./node 2 &
./node 3 &
./node 4 &

gcc -o hacker hacker.c
./hacker
