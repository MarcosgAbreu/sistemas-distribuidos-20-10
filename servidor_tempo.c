#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define MY_PORT_ID 6090
#define BLOQUEIO_RECURSO_S 2

/* Semáforo binário: valor 1 = recurso livre, 0 = ocupado. */
typedef struct {
    int valor;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} semaforo_t;

static semaforo_t recurso;

/* Prepara o semáforo com o valor inicial e os primitivos de espera. */
static void semaforo_init(semaforo_t *s, int valor)
{
    s->valor = valor;
    pthread_mutex_init(&s->mutex, NULL);
    pthread_cond_init(&s->cond, NULL);
}

/* Entra na seção crítica. Se o recurso estiver ocupado, a thread dorme. */
static void semaforo_wait(semaforo_t *s)
{
    pthread_mutex_lock(&s->mutex);
    while (s->valor <= 0)
        pthread_cond_wait(&s->cond, &s->mutex);
    s->valor--;
    pthread_mutex_unlock(&s->mutex);
}

/* Libera o recurso e acorda uma thread que esteja esperando. */
static void semaforo_post(semaforo_t *s)
{
    pthread_mutex_lock(&s->mutex);
    s->valor++;
    pthread_cond_signal(&s->cond);
    pthread_mutex_unlock(&s->mutex);
}

/* Imprime o timestamp cru e a mesma hora no fuso local. */
static void imprime_hora(const char *rotulo, time_t sec)
{
    struct tm tm;
    char buf[32];

    localtime_r(&sec, &tm);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    printf("%s %ld (%s)\n", rotulo, (long)sec, buf);
    fflush(stdout);
}

/* Lê ou escreve o buffer inteiro. read/write podem entregar só um pedaço. */
static int transferir_tudo(int fd, void *buf, size_t n, int escrever)
{
    size_t off = 0;

    while (off < n) {
        ssize_t r = escrever
            ? write(fd, (char *)buf + off, n - off)
            : read(fd, (char *)buf + off, n - off);
        if (r == 0)
            return -1;
        if (r < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        off += (size_t)r;
    }
    return 0;
}

/* Atende um cliente. Várias threads rodam juntas; o semáforo serializa a hora. */
static void *atende(void *arg)
{
    int newsockid = *(int *)arg;
    struct timeval tp;
    struct timezone tzp;

    free(arg);

    /* Recebe a hora do cliente e converte para a ordem da máquina. */
    if (transferir_tudo(newsockid, &tp, sizeof(tp), 0) < 0) {
        printf("error reading new socket\n");
        close(newsockid);
        return NULL;
    }

    tp.tv_sec = (time_t)ntohl((uint32_t)tp.tv_sec);
    tp.tv_usec = (suseconds_t)ntohl((uint32_t)tp.tv_usec);
    imprime_hora("server: remote time is", tp.tv_sec);

    /* Só um cliente por vez. Quem chega depois fica parado neste wait. */
    semaforo_wait(&recurso);

    /* Pega a hora local, converte para a rede e devolve ao cliente. */
    gettimeofday(&tp, &tzp);
    imprime_hora("server: local time is", tp.tv_sec);

    tp.tv_sec = (time_t)htonl((uint32_t)tp.tv_sec);
    tp.tv_usec = (suseconds_t)htonl((uint32_t)tp.tv_usec);
    if (transferir_tudo(newsockid, &tp, sizeof(tp), 1) < 0)
        printf("error writing new socket\n");

    close(newsockid);

    /* Segura o recurso por 2s. O próximo cliente só entra depois do post. */
    sleep(BLOQUEIO_RECURSO_S);
    semaforo_post(&recurso);
    return NULL;
}

int main(void)
{
    int sockid, opt = 1;
    struct sockaddr_in ssock_addr;

    /* Recurso começa livre: o primeiro cliente entra direto. */
    semaforo_init(&recurso, 1);

    /* Abre o socket TCP e permite reusar a porta se o servidor reiniciar. */
    if ((sockid = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("erro criação= %d\n", errno);
        exit(1);
    }

    setsockopt(sockid, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* Escuta em todas as interfaces, na porta 6090. */
    memset(&ssock_addr, 0, sizeof(ssock_addr));
    ssock_addr.sin_family = AF_INET;
    ssock_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    ssock_addr.sin_port = htons(MY_PORT_ID);

    if (bind(sockid, (struct sockaddr *)&ssock_addr, sizeof(ssock_addr)) < 0) {
        printf("error binding socket, error:%d\n", errno);
        exit(1);
    }

    if (listen(sockid, 128) < 0) {
        printf("erro listening: %d\n", errno);
        exit(1);
    }

    /* Aceita conexões sem parar. Cada uma vira uma thread de atendimento. */
    for (;;) {
        int newsockid = accept(sockid, NULL, NULL);
        int *pfd;
        pthread_t tid;

        if (newsockid < 0) {
            if (errno == EINTR)
                continue;
            printf("error accepting socket, error: %d\n", errno);
            exit(1);
        }

        /* O descritor vai no heap para a thread não ler uma variável da pilha. */
        pfd = malloc(sizeof(*pfd));
        if (pfd == NULL) {
            close(newsockid);
            continue;
        }
        *pfd = newsockid;

        if (pthread_create(&tid, NULL, atende, pfd) != 0) {
            close(newsockid);
            free(pfd);
            continue;
        }
        pthread_detach(tid);
    }
}
