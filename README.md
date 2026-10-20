# Servidor de hora com semáforo

Trabalho de sistemas distribuídos. Vários clientes pedem a hora para o mesmo servidor. O servidor atende um por vez e, depois de responder, segura o recurso por 2 segundos. O próximo cliente só recebe a hora quando esse tempo acaba.

Os fontes são o `cliente_tempo.c` e o `servidor_tempo.c`.

## Como funciona

O cliente abre um socket TCP, conecta em `127.0.0.1` na porta `6090` e manda a hora da própria máquina (`gettimeofday`). Depois fica esperando a resposta. Quando chega, ele imprime as duas horas: a local e a do servidor. Cada uma aparece do jeito cru (segundos desde 01/01/1970) e também formatada (`AAAA-MM-DD HH:MM:SS`).

O servidor fica em loop aceitando conexões. Cada cliente vira uma thread, então vários podem estar conectados ao mesmo tempo. Quem controla o atendimento é um semáforo binário:

- valor `1`: o recurso está livre
- valor `0`: alguém está sendo atendido

O primeiro cliente passa direto. Ele recebe a hora local do servidor e a conexão fecha. A thread ainda segura o semáforo e dorme 2 segundos (`sleep`). Só então faz o `post` e libera o próximo. O segundo cliente, que já estava parado no `wait`, entra, é atendido, e segura de novo por mais 2 segundos. E assim por diante.

A leitura da hora que o cliente enviou acontece antes do semáforo. O que fica exclusivo é a hora do servidor: pegar o relógio, mandar para o cliente e esperar os 2 segundos.

## Como executar

Na pasta do trabalho:

```bash
gcc -Wall -Wextra -o cliente_tempo cliente_tempo.c
gcc -Wall -Wextra -pthread -o servidor_tempo servidor_tempo.c
```

O servidor precisa da flag `-pthread` por causa das threads e do semáforo.

Sobe o servidor num terminal e deixa ele rodando:

```bash
./servidor_tempo
```

Em outros terminais, um cliente por vez, ou vários juntos:

```bash
./cliente_tempo
```

Para ver o bloqueio, abre três terminais e dispara os três clientes quase ao mesmo tempo. O primeiro responde na hora. O segundo sai cerca de 2 segundos depois. O terceiro, cerca de 4.

Exemplo do que o cliente imprime:

```text
Criando struct addr p/ server
client: local time is 1792459446 (2026-10-19 22:24:06)
client: remote time is 1792459448 (2026-10-19 22:24:08)
```

`local time` é a hora da máquina do cliente, no momento em que ele pediu. `remote time` é a hora do servidor, no momento em que aquele cliente foi atendido. Se a segunda estiver 2 segundos na frente, esse cliente esperou o anterior soltar o recurso.

Para encerrar o servidor, `Ctrl+C` no terminal dele.

Se a porta `6090` já estiver em uso, o `bind` falha. Mata o servidor antigo e sobe de novo.
