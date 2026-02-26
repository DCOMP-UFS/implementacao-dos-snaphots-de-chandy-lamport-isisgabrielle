Implementação dos Snapshots de Chandy/Lamport feita por Júlia Valverde, Isis Gabrielle e Samuel Guimarães.
O arquivo que deve ser compilado e rodado é o etapa4.c
Comandos para rodar: 
Caso ainda não tenha o compilador e as bibliotecas do MPI instaladas no seu ambiente (Ubuntu/Debian, WSL ou Codespaces), execute o comando abaixo no terminal:

```bash
sudo apt update && sudo apt install -y openmpi-bin libopenmpi-dev

Compilar:
mpicc -o snapshot etapa4.c -lpthread

Executar:
mpirun --oversubscribe -np 3 ./snapshot
