// Krown Agent - Daemon SSH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <sys/stat.h>

#include "agent.h"
#include "ssh_handler.h"
#include "socket_server.h"
#include "request_handler.h"

static volatile bool running = true;
static int server_fd = -1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        DEBUG_PRINT("\n[Agent] Signal de terminaison reçu\n");
        running = false;
        if (server_fd >= 0) close(server_fd);
    }
}

int main(int argc, char *argv[]) {
    DEBUG_PRINT("=== Krown Agent v1.0 ===\n");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (ssh_handler_init() != 0) {
        fprintf(stderr, "[Agent] Erreur: Échec de l'initialisation SSH\n");
        return 1;
    }

    const char *socket_path = getenv("SOCKET_PATH");
    if (!socket_path) socket_path = "/tmp/krown-agent.sock";
    if (argc > 1) socket_path = argv[1];

    DEBUG_PRINT("[Agent] Écoute sur socket: %s\n", socket_path);
    
    server_fd = socket_server_start(socket_path);
    if (server_fd < 0) {
        fprintf(stderr, "[Agent] Erreur: Impossible de démarrer le serveur socket\n");
        ssh_handler_cleanup();
        return 1;
    }

    DEBUG_PRINT("[Agent] Daemon prêt\n");

    // Boucle principale avec select() pour éviter les appels accept() inutiles
    // Accepter plusieurs connexions par itération pour améliorer le débit
    while (running) {
        fd_set read_fds;
        struct timeval timeout;
        
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);
        
        // Timeout de 1 seconde pour permettre la vérification de 'running'
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int select_result = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (select_result < 0) {
            if (errno == EINTR) {
                // Interruption par signal, continuer
                continue;
            }
            if (running) {
                perror("[Agent] Erreur select");
            }
            continue;
        }
        
        if (select_result == 0) {
            // Timeout - pas de connexion en attente, continuer la boucle
            continue;
        }
        
        // Accepter plusieurs connexions en attente (jusqu'à 10 par itération)
        int accepted = 0;
        while (accepted < 10 && running) {
            // Vérifier que le socket est vraiment prêt
            if (!FD_ISSET(server_fd, &read_fds)) {
                // Re-vérifier avec select() si nécessaire
                FD_ZERO(&read_fds);
                FD_SET(server_fd, &read_fds);
                timeout.tv_sec = 0;
                timeout.tv_usec = 0;
                if (select(server_fd + 1, &read_fds, NULL, NULL, &timeout) <= 0) {
                    break;
                }
            }
            
            int client_fd = socket_server_accept(server_fd);
            if (client_fd < 0) {
                // EAGAIN/EWOULDBLOCK peut se produire si la connexion est fermée
                // entre select() et accept(), ou dans des cas de race condition
                // On ignore silencieusement ces erreurs
                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != ECONNABORTED && running) {
                    perror("[Agent] Erreur accept");
                }
                break;
            }

            // Traiter la requête dans un thread séparé
            pthread_t thread;
            int *client_ptr = malloc(sizeof(int));
            if (!client_ptr) {
                close(client_fd);
                break;
            }
            *client_ptr = client_fd;
            
            if (pthread_create(&thread, NULL, handle_client_request, client_ptr) != 0) {
                perror("[Agent] Erreur création thread");
                close(client_fd);
                free(client_ptr);
                break;
            } else {
                pthread_detach(thread);
                accepted++;
            }
        }
    }

    DEBUG_PRINT("[Agent] Arrêt du daemon...\n");
    socket_server_stop(server_fd, socket_path);
    ssh_handler_cleanup();
    return 0;
}

