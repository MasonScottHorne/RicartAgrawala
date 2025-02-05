# Ricart-Agrawala Distributed Mutual Exclusion Algorithm
Ricart, G., & Agrawala, A. K. (1981). An Optimal Algorithm for Mutual Exclusion in Computer Networks.
https://dl.acm.org/doi/pdf/10.1145/358527.358537
## Compilation
```sh

Terminal 1:
gcc -o print print.c
./print

Terminal 2:
gcc -o node node.c
./node 1 &

Terminal 3:
./node 2 &

Terminal 4:
./node 3 &

Terminal 5:
./node 4 &

Terminal 6:
gcc -o hacker hacker.c
./hacker
