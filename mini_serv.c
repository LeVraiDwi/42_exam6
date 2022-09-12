#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>

int SOCK_FD[__FD_SETSIZE];

int CL_ID[__FD_SETSIZE];
size_t CL_CNT = 0;

fd_set FDSET_ALL = {};
fd_set FDSET_RD, FDSET_WR;

char *READY_Q[__FD_SETSIZE] = {};
char *PENDING_Q[__FD_SETSIZE] = {};

void *ft_memcpy(void *dst, const void *src, size_t n)
{
    char *sdst = dst;
    const char *ssrc = src;

    while (n--)
    {
        sdst[n] = ssrc[n];
    }
    return dst;
}

//retire et libere les socket des queues
void clear_queues(void){
    for(size_t i = 1; i <= CL_CNT; i++)
    {
        free(READY_Q[SOCK_FD[i]]);
        free(PENDING_Q[SOCK_FD[i]]);
    }
    return;
}

//close les socket
void clear_sockets(void)
{
    for(size_t i = 0; i <= CL_CNT; ++i)
        close(SOCK_FD[i]);
}


//affiche erreur et clean les socket
void fatal_error(void)
{
    clear_queues();
    clear_sockets();
    write(2, "Fatal error\n", 12);
    exit(1);
}

//reattribu l'espace necessesaire pour la nouvelle chaine de caractere de la queue
void str_reserve(char **sp, size_t new_size)
{
    bool empty = !*sp;
    char *tmp = realloc(*sp, new_size);
    
    if (!tmp)
        fatal_error();
    if (empty)
    *tmp = 0;
    *sp = tmp;
}

//concataine la chaine et de queue avec un nouvelle chaine;
void str_append(char **dst, char *src)
{
    str_reserve(dst, (*dst ? strlen(*dst) : 0) + strlen(src) + 1); //aloue le nouvelle espace pour la chaine
    strcat(*dst, src); //concataine les 2 chaine
}

//ajoute a la queue les message s pour chaque utilisateur autre que les sender
void append_to_queues(char **queue, char *s, size_t sender_pos)
{
    size_t i = 0;
    //ajoute pour chauqe utilisateur un message
    while (++i != sender_pos)
        str_append(&queue[SOCK_FD[i]], s);
    while(++i <= CL_CNT)
        str_append(&queue[SOCK_FD[i]], s);
}

//retire un client
void remove_client(int pos)
{
    FD_CLR(SOCK_FD[pos], &FDSET_ALL); //retire de fdset_all le sock_fd a retirer des client
    close(SOCK_FD[pos]); //ferme sock_fd

    char tmpbuf[128];
    sprintf(tmpbuf, "server: client %u just left\n", CL_ID[SOCK_FD[pos]]);
    append_to_queues(READY_Q, tmpbuf, pos); //ajoute au message a send que le client est parti
    SOCK_FD[pos] = SOCK_FD[CL_CNT--]; //remplace le client par le client suivant et enleve 1 au client count
}

int main (int argc, char **argv)
{
    //verifie que l' on est le bon nombre d'argument
    if (argc < 2)
    {
        write (2, "Wrong number of arguments\n", 26);
        return 1;
    }

    //setup la structure addresse
    struct sockaddr_in servaddr = {
        .sin_family = AF_INET,
        .sin_addr.s_addr = htonl(2130706433), // equivaux a 127.0.0.1
        .sin_port = htons(atoi(argv[1])) // attribu le port donner par le user
    };
    uint32_t last_id = 0;

    SOCK_FD[0] = socket(AF_INET, SOCK_STREAM, 0); //creer mon socket
    if (SOCK_FD[0] == -1)
        fatal_error();
    if (bind(SOCK_FD[0], (void *)&servaddr, sizeof(servaddr)) == -1) //lie le socket a l' adresse definie dans servaddr
        fatal_error();
    if (listen(SOCK_FD[0], 32)) //ecoute le socket
        fatal_error();

    FD_SET(SOCK_FD[0], &FDSET_ALL); //ajout mon socket dans ma liste de socket
    while (true)
    {
        FDSET_RD = FDSET_WR = FDSET_ALL;
        if (select(__FD_SETSIZE, &FDSET_RD, &FDSET_WR, NULL, NULL) == -1) //attend qu'un set soit pret en lecture/ecriture
            fatal_error();

        if (FD_ISSET(SOCK_FD[0], &FDSET_RD)) //si sock_fd[0] est pret a etre lu il y a un nouvelle connexion
        {
            SOCK_FD[++CL_CNT] = accept(SOCK_FD[0], NULL, NULL); //accept une nouvelle connexion
            if (SOCK_FD[CL_CNT] == -1) 
                fatal_error();
            CL_ID[SOCK_FD[CL_CNT]] = last_id++; //j'ajoute la nouvelle connexion a ma liste de client
            FD_SET(SOCK_FD[CL_CNT], &FDSET_ALL); // ajoute le client a mon set de socket

            char tmpbuf[128];
            sprintf(tmpbuf, "server: client %u just arrived\n", CL_ID[SOCK_FD[CL_CNT]]); 
            append_to_queues(READY_Q, tmpbuf, CL_CNT); //ajoute le message d'arriver du client au message a envoyer
        }

        for (size_t i = 1; i <= CL_CNT; ++i) // on parcourt les client (pas sock_fd[0])
        {
            if (FD_ISSET(SOCK_FD[i], &FDSET_RD)) //si le client est pret a etre lu
            {
                char rdbuf[1024 + 1];
                const int rd_set = recv(SOCK_FD[i], rdbuf, 1024, 0); //recois le message

                if (rd_set <= 0)
                {
                    remove_client(i--); //si le message est null ou erreur se produit retire le client
                    continue;
                }
                rdbuf[rd_set] = 0; // on null term le buffer
                append_to_queues(PENDING_Q, rdbuf, i); // et on ajoute le message au message en attente d'envoyer

                char tmpbuf[128];
                sprintf(tmpbuf, "client %u: ", CL_ID[SOCK_FD[i]]); // on creer l'entete du fichier
                char *nl;

                for(size_t j = 1; j <= CL_CNT; ++j){ // on envoie le message au autre client
                    if (i == j)
                        continue;
                    while((nl = strstr(PENDING_Q[SOCK_FD[j]], "\n"))) // on decoupe la chaine a chaque \n
                    {
                        const size_t len = nl - PENDING_Q[SOCK_FD[j]] + 1; // on determine la longueur de la chaine jusqu'a sont \n
                        char slice[len + 1];
                        slice[len] = 0;
                        
                        ft_memcpy(slice, PENDING_Q[SOCK_FD[j]], len); //on copie la partie de chaine dans slice;
                        str_append(&READY_Q[SOCK_FD[j]], tmpbuf);//on ajoute l' entete au message a envoyer
                        str_append(&READY_Q[SOCK_FD[j]], slice); //on ajoute le message ensuite

                        strcpy(PENDING_Q[SOCK_FD[j]], nl + 1); // remplace la chaine d' origine par la copie du reste de la chaine
                        str_reserve(&PENDING_Q[SOCK_FD[j]], strlen(PENDING_Q[SOCK_FD[j]]) + 1); //realloue l'espace pour eviter perte memoire
                    }
                }
            }
            if (READY_Q[SOCK_FD[i]] && FD_ISSET(SOCK_FD[i], &FDSET_WR)) // si le socket a des message pret a envoyer et qu'il est pret a ecrire
            {
                const size_t len = strlen(READY_Q[SOCK_FD[i]]); //la taille du message
                const int wr_ret = send(SOCK_FD[i], READY_Q[SOCK_FD[i]], len, MSG_NOSIGNAL); //envoie du message

                if (wr_ret == -1) //si envoie echoue on retire le client
                {
                    remove_client(i--);
                    continue;
                }
                free(READY_Q[SOCK_FD[i]]); //on libere le mesasge envoyer
                READY_Q[SOCK_FD[i]] = NULL; //on met les message a envoyer a null
            }
        }
    }
    return 0;
}