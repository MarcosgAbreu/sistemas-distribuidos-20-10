#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>
#define SERV_PORT_ID 6090

/* Imprime o timestamp cru e a mesma hora no fuso local. */
static void imprime_hora(const char *rotulo, time_t sec)
{
 char buf[32];
 struct tm tm;

 localtime_r(&sec, &tm);
 strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
 printf("  %s\n", rotulo);
 printf("    %s\n", buf);
 printf("    %ld s desde 1970\n\n", (long)sec);
 fflush(stdout);
}

int main(int argc, char **argv)
{
 int sockid;
 struct sockaddr_in ssock_addr;
 struct timeval tp;
 struct timezone tzp;
 in_addr_t ip;

 if (argc != 2) {
     printf("uso: %s <ip-do-servidor>\n", argv[0]);
     exit(1);
 }
 ip = inet_addr(argv[1]);
 if (ip == (in_addr_t)INADDR_NONE) {
     printf("ip invalido: %s\n", argv[1]);
     exit(1);
 }

 /* Abre um socket TCP. */
 if ((sockid = socket(AF_INET,SOCK_STREAM, 0)) < 0){
     printf("erro criacao=%d",errno);
     exit(0);
 }

 /* Preenche o endereço do servidor e conecta. */
 bzero((char *)&ssock_addr,sizeof(ssock_addr));
 ssock_addr.sin_family = AF_INET;
 ssock_addr.sin_addr.s_addr = ip;
 ssock_addr.sin_port = htons(SERV_PORT_ID);
 if (connect(sockid, (struct sockaddr *) &ssock_addr, sizeof(ssock_addr)) < 0){
    printf("error connecting to server, error:  %d\n",errno);
    exit(0);
 }

 printf("\nconectado em %s, porta %d\n\n", argv[1], SERV_PORT_ID);

 /* Lê a hora local, mostra na tela e converte para ordem de rede. */
 gettimeofday(&tp,&tzp);
 imprime_hora("hora desta maquina", tp.tv_sec);
 tp.tv_sec = (time_t)htonl((uint32_t)tp.tv_sec);
 tp.tv_usec = (suseconds_t)htonl((uint32_t)tp.tv_usec);

 /* Envia a hora local e espera a hora do servidor. */
 write(sockid, &tp, sizeof(tp));
 if ((read(sockid, &tp, sizeof(tp))) < 0){
     printf("error reading new socket\n");
     exit(0);
 }

 /* Converte de volta para a ordem da máquina e imprime a hora recebida. */
 tp.tv_sec = (time_t)ntohl((uint32_t)tp.tv_sec);
 tp.tv_usec = (suseconds_t)ntohl((uint32_t)tp.tv_usec);
 imprime_hora("hora do servidor", tp.tv_sec);
 close(sockid);
 return 0;
}
