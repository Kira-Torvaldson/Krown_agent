/**
 * Gestionnaire SSH - Utilise libssh pour les connexions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <stdbool.h>
#include <json-c/json.h>
#include <libssh/libssh.h>

#include "ssh_handler.h"
#include "agent.h"

#include "memory.h"

// Macros JSON
#define JSON_PARSE_OR_RETURN(json_str, root_var, error_msg) \
    do { \
        root_var = json_tokener_parse(json_str); \
        if (!root_var) { \
            *response = strdup("{\"error\":\"" error_msg "\"}"); \
            return RESP_ERROR; \
        } \
    } while(0)

#define JSON_GET_STRING_OR_RETURN(root_var, key, var, error_msg) \
    do { \
        json_object *obj; \
        if (!json_object_object_get_ex(root_var, key, &obj) || !obj) { \
            json_object_put(root_var); \
            *response = strdup("{\"error\":\"" error_msg "\"}"); \
            return RESP_ERROR; \
        } \
        var = json_object_get_string(obj); \
    } while(0)

#define MAX_SESSIONS 100

// Structure de session SSH
typedef struct {
    char session_id[64];
    ssh_session session;
    bool connected;
    time_t created_at;
} ssh_session_t;

static ssh_session_t sessions[MAX_SESSIONS];
static int session_count = 0;
static pthread_mutex_t sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * Initialiser le gestionnaire SSH
 */
int ssh_handler_init(void) {
    ssh_init();
    memset(sessions, 0, sizeof(sessions));
    session_count = 0;
    DEBUG_PRINT("[SSH] Gestionnaire initialisé\n");
    return 0;
}

/**
 * Nettoyer le gestionnaire SSH
 */
void ssh_handler_cleanup(void) {
    pthread_mutex_lock(&sessions_mutex);
    
    // Fermer toutes les sessions
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].connected && sessions[i].session) {
            ssh_disconnect(sessions[i].session);
            ssh_free(sessions[i].session);
        }
    }
    
    session_count = 0;
    pthread_mutex_unlock(&sessions_mutex);
    ssh_finalize();
    DEBUG_PRINT("[SSH] Gestionnaire nettoyé\n");
}

/**
 * Trouver une session par ID
 * Retourne un pointeur vers la session (le mutex reste verrouillé)
 * L'appelant DOIT déverrouiller le mutex après utilisation
 */
static ssh_session_t* find_session_locked(const char *session_id) {
    for (int i = 0; i < session_count; i++) {
        if (strcmp(sessions[i].session_id, session_id) == 0) {
            return &sessions[i];
        }
    }
    return NULL;
}

/**
 * Trouver une session par ID (version thread-safe)
 */
static ssh_session_t* find_session(const char *session_id) {
    pthread_mutex_lock(&sessions_mutex);
    ssh_session_t *sess = find_session_locked(session_id);
    pthread_mutex_unlock(&sessions_mutex);
    return sess;
}

/**
 * Gérer la connexion SSH
 */
response_code_t handle_ssh_connect(const char *json_data, char **response) {
    if (!json_data || !response) {
        if (response) *response = strdup("{\"error\":\"Paramètres invalides\"}");
        return RESP_ERROR;
    }
    
    json_object *root;
    JSON_PARSE_OR_RETURN(json_data, root, "JSON invalide");

    json_object *host_obj, *port_obj, *user_obj, *pass_obj, *key_obj, *passphrase_obj;
    const char *host, *username, *password = NULL, *private_key = NULL, *passphrase = NULL;
    int port = 22;

    json_object_object_get_ex(root, "host", &host_obj);
    json_object_object_get_ex(root, "username", &user_obj);
    json_object_object_get_ex(root, "port", &port_obj);
    json_object_object_get_ex(root, "password", &pass_obj);
    json_object_object_get_ex(root, "private_key", &key_obj);
    json_object_object_get_ex(root, "passphrase", &passphrase_obj);

    if (!host_obj || !user_obj) {
        json_object_put(root);
        *response = strdup("{\"error\":\"host et username requis\"}");
        return RESP_ERROR;
    }

    host = json_object_get_string(host_obj);
    username = json_object_get_string(user_obj);
    if (port_obj) port = json_object_get_int(port_obj);
    if (pass_obj) {
        password = json_object_get_string(pass_obj);
        printf("[SSH] Mot de passe reçu (longueur: %zu)\n", password ? strlen(password) : 0);
    }
    if (key_obj) private_key = json_object_get_string(key_obj);
    if (passphrase_obj) {
        passphrase = json_object_get_string(passphrase_obj);
        printf("[SSH] Passphrase reçue (longueur: %zu)\n", passphrase ? strlen(passphrase) : 0);
    }

    // Créer la session SSH
    ssh_session session = ssh_new();
    if (!session) {
        json_object_put(root);
        *response = strdup("{\"error\":\"Impossible de créer la session SSH\"}");
        return RESP_SSH_ERROR;
    }

    ssh_options_set(session, SSH_OPTIONS_HOST, host);
    ssh_options_set(session, SSH_OPTIONS_PORT, &port);
    ssh_options_set(session, SSH_OPTIONS_USER, username);

    // Connexion
    int rc = ssh_connect(session);
    if (rc != SSH_OK) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "{\"error\":\"Échec connexion: %s\"}", ssh_get_error(session));
        *response = strdup(error_msg);
        ssh_free(session);
        json_object_put(root);
        return RESP_SSH_ERROR;
    }

    int auth_methods = ssh_userauth_list(session, username);
    DEBUG_PRINT("[SSH] Authentification %s@%s:%d\n", username, host, port);
    
    if (password && strlen(password) > 0) {
        if (!(auth_methods & SSH_AUTH_METHOD_PASSWORD)) {
            DEBUG_PRINT("[SSH] ERREUR: Le serveur n'accepte pas l'authentification par mot de passe\n");
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), 
                    "{\"error\":\"Le serveur SSH n'accepte pas l'authentification par mot de passe\"}");
            *response = strdup(error_msg);
            ssh_disconnect(session);
            ssh_free(session);
            json_object_put(root);
            return RESP_SSH_ERROR;
        }
        
        rc = ssh_userauth_password(session, NULL, password);
        
        if (rc == SSH_AUTH_SUCCESS) {
            printf("[SSH] Authentification par mot de passe réussie\n");
        } else {
            printf("[SSH] Échec authentification par mot de passe: %s (code: %d)\n", 
                   ssh_get_error(session), rc);
            
            // Essayer d'obtenir plus d'informations sur l'erreur
            if (rc == SSH_AUTH_DENIED) {
                printf("[SSH] Accès refusé - le mot de passe est peut-être incorrect\n");
            } else if (rc == SSH_AUTH_PARTIAL) {
                printf("[SSH] Authentification partielle - méthode supplémentaire requise\n");
            }
        }
    } else if (private_key && strlen(private_key) > 0) {
        printf("[SSH] Méthode: clé privée (longueur: %zu)\n", strlen(private_key));
        
        // Vérifier que le serveur accepte l'authentification par clé publique
        if (!(auth_methods & SSH_AUTH_METHOD_PUBLICKEY)) {
            printf("[SSH] ERREUR: Le serveur n'accepte pas l'authentification par clé publique\n");
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), 
                    "{\"error\":\"Le serveur SSH n'accepte pas l'authentification par clé publique\"}");
            *response = strdup(error_msg);
            ssh_disconnect(session);
            ssh_free(session);
            json_object_put(root);
            return RESP_SSH_ERROR;
        }
        
        char tmp_key_file[] = "/tmp/krown_ssh_key_XXXXXX";
        int tmp_fd = mkstemp(tmp_key_file);
        if (tmp_fd < 0) {
            DEBUG_PRINT("[SSH] ERREUR: Impossible de créer un fichier temporaire pour la clé\n");
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), 
                    "{\"error\":\"Impossible de créer un fichier temporaire pour la clé privée\"}");
            *response = strdup(error_msg);
            ssh_disconnect(session);
            ssh_free(session);
            json_object_put(root);
            return RESP_SSH_ERROR;
        }
        
        // Écrire la clé privée dans le fichier temporaire
        ssize_t written = write(tmp_fd, private_key, strlen(private_key));
        close(tmp_fd);
        
        if (written != (ssize_t)strlen(private_key)) {
            DEBUG_PRINT("[SSH] ERREUR: Impossible d'écrire la clé privée\n");
            unlink(tmp_key_file);
            char error_msg[256];
            snprintf(error_msg, sizeof(error_msg), 
                    "{\"error\":\"Impossible d'écrire la clé privée dans le fichier temporaire\"}");
            *response = strdup(error_msg);
            ssh_disconnect(session);
            ssh_free(session);
            json_object_put(root);
            return RESP_SSH_ERROR;
        }
        
        // Changer les permissions du fichier (lecture seule pour le propriétaire)
        chmod(tmp_key_file, 0600);
        
        // Importer la clé privée (avec passphrase si fournie)
        ssh_key privkey = NULL;
        int import_rc = ssh_pki_import_privkey_file(tmp_key_file, passphrase, NULL, NULL, &privkey);
        
        // Supprimer le fichier temporaire immédiatement après import
        unlink(tmp_key_file);
        
        if (import_rc != SSH_OK || privkey == NULL) {
            DEBUG_PRINT("[SSH] ERREUR: Impossible d'importer la clé privée\n");
            char error_msg[512];
            snprintf(error_msg, sizeof(error_msg), 
                    "{\"error\":\"Impossible d'importer la clé privée: format invalide ou clé corrompue. Vérifiez le format de la clé (OpenSSH, PEM, etc.) et la passphrase si nécessaire.\"}");
            *response = strdup(error_msg);
            ssh_disconnect(session);
            ssh_free(session);
            json_object_put(root);
            return RESP_SSH_ERROR;
        }
        
        // Extraire la clé publique pour le débogage
        ssh_key pubkey = NULL;
        if (ssh_pki_export_privkey_to_pubkey(privkey, &pubkey) == SSH_OK && pubkey != NULL) {
            char *pubkey_str = NULL;
            if (ssh_pki_export_pubkey_base64(pubkey, &pubkey_str) == SSH_OK && pubkey_str != NULL) {
                // Afficher les 50 premiers caractères de la clé publique pour le débogage
                char pubkey_preview[64] = {0};
                strncpy(pubkey_preview, pubkey_str, 50);
                printf("[SSH] Clé publique (preview): %s...\n", pubkey_preview);
                printf("[SSH] Vérifiez que cette clé publique est dans ~/.ssh/authorized_keys sur le serveur\n");
                ssh_string_free_char(pubkey_str);
            }
            ssh_key_free(pubkey);
        }
        
        // Essayer d'abord avec ssh_userauth_try_publickey pour vérifier si la clé est acceptée
        int try_rc = ssh_userauth_try_publickey(session, NULL, privkey);
        if (try_rc == SSH_AUTH_SUCCESS) {
            printf("[SSH] La clé publique est acceptée par le serveur, tentative d'authentification...\n");
        } else if (try_rc == SSH_AUTH_DENIED) {
            printf("[SSH] ATTENTION: La clé publique n'est PAS dans authorized_keys sur le serveur\n");
            printf("[SSH] Vérifiez que la clé publique correspondante est dans ~/.ssh/authorized_keys\n");
        } else {
            printf("[SSH] Résultat du test de la clé: code %d\n", try_rc);
        }
        
        // Authentifier avec la clé privée
        rc = ssh_userauth_publickey(session, NULL, privkey);
        
        // Libérer la clé
        ssh_key_free(privkey);
        
        if (rc == SSH_AUTH_SUCCESS) {
            printf("[SSH] Authentification par clé privée réussie\n");
        } else {
            printf("[SSH] Échec authentification par clé privée: %s (code: %d)\n", 
                   ssh_get_error(session), rc);
            
            if (rc == SSH_AUTH_DENIED) {
                printf("[SSH] Accès refusé - causes possibles:\n");
                printf("[SSH]   1. La clé publique n'est pas dans ~/.ssh/authorized_keys sur le serveur\n");
                printf("[SSH]   2. Les permissions de ~/.ssh ou authorized_keys sont incorrectes (doivent être 700 et 600)\n");
                printf("[SSH]   3. La clé privée ne correspond pas à la clé publique dans authorized_keys\n");
                printf("[SSH]   4. Le serveur SSH a désactivé l'authentification par clé publique\n");
            } else if (rc == SSH_AUTH_PARTIAL) {
                printf("[SSH] Authentification partielle - méthode supplémentaire requise\n");
            } else if (rc == SSH_AUTH_ERROR) {
                printf("[SSH] Erreur lors de l'authentification - vérifiez les logs du serveur SSH\n");
            }
        }
    } else {
        printf("[SSH] Méthode: clé publique automatique\n");
        rc = ssh_userauth_publickey_auto(session, NULL, NULL);
        
        if (rc == SSH_AUTH_SUCCESS) {
            printf("[SSH] Authentification par clé publique réussie\n");
        } else {
            printf("[SSH] Échec authentification par clé publique: %s (code: %d)\n", 
                   ssh_get_error(session), rc);
        }
    }

    if (rc != SSH_AUTH_SUCCESS) {
        const char *error_str = ssh_get_error(session);
        char error_msg[512];
        const char *error_detail = (rc == SSH_AUTH_DENIED) ? 
            (private_key && strlen(private_key) > 0 ? " Clé publique non autorisée." : " Identifiants incorrects.") :
            (rc == SSH_AUTH_PARTIAL ? " Authentification partielle." : " Erreur d'authentification.");
        snprintf(error_msg, sizeof(error_msg), 
                "{\"error\":\"Échec authentification: %s%s\",\"auth_code\":%d}", 
                error_str, error_detail, rc);
        *response = strdup(error_msg);
        ssh_disconnect(session);
        ssh_free(session);
        json_object_put(root);
        return RESP_SSH_ERROR;
    }

    // Enregistrer la session
    pthread_mutex_lock(&sessions_mutex);
    if (session_count < MAX_SESSIONS) {
        char session_id[64];
        snprintf(session_id, sizeof(session_id), "session_%d_%ld", session_count, time(NULL));
        
        strncpy(sessions[session_count].session_id, session_id, sizeof(sessions[session_count].session_id) - 1);
        sessions[session_count].session = session;
        sessions[session_count].connected = true;
        sessions[session_count].created_at = time(NULL);
        session_count++;

        char response_json[512];
        snprintf(response_json, sizeof(response_json), 
                "{\"session_id\":\"%s\",\"status\":\"connected\",\"host\":\"%s\",\"port\":%d}",
                session_id, host, port);
        *response = strdup(response_json);
        pthread_mutex_unlock(&sessions_mutex);
        json_object_put(root);
        return RESP_OK;
    }
    pthread_mutex_unlock(&sessions_mutex);

    ssh_disconnect(session);
    ssh_free(session);
    json_object_put(root);
    *response = strdup("{\"error\":\"Nombre maximum de sessions atteint\"}");
    return RESP_ERROR;
}

/**
 * Gérer la déconnexion SSH
 */
response_code_t handle_ssh_disconnect(const char *json_data, char **response) {
    if (!json_data || !response) {
        if (response) *response = strdup("{\"error\":\"Paramètres invalides\"}");
        return RESP_ERROR;
    }
    
    json_object *root;
    JSON_PARSE_OR_RETURN(json_data, root, "JSON invalide");

    const char *session_id;
    JSON_GET_STRING_OR_RETURN(root, "session_id", session_id, "session_id requis");
    ssh_session_t *sess = find_session(session_id);

    if (!sess || !sess->connected) {
        json_object_put(root);
        *response = strdup("{\"error\":\"Session introuvable\"}");
        return RESP_ERROR;
    }

    ssh_disconnect(sess->session);
    ssh_free(sess->session);
    sess->connected = false;

    *response = strdup("{\"status\":\"disconnected\"}");
    json_object_put(root);
    return RESP_OK;
}

/**
 * Gérer l'exécution de commande SSH
 */
response_code_t handle_ssh_execute(const char *json_data, char **response) {
    if (!json_data || !response) {
        if (response) *response = strdup("{\"error\":\"Paramètres invalides\"}");
        return RESP_ERROR;
    }
    
    json_object *root;
    JSON_PARSE_OR_RETURN(json_data, root, "JSON invalide");

    const char *session_id;
    JSON_GET_STRING_OR_RETURN(root, "session_id", session_id, "session_id requis");
    
    const char *command;
    JSON_GET_STRING_OR_RETURN(root, "command", command, "command requis");
    
    ssh_session_t *sess = find_session(session_id);
    if (!sess || !sess->connected) {
        json_object_put(root);
        *response = strdup("{\"error\":\"Session introuvable ou déconnectée\"}");
        return RESP_ERROR;
    }

    // Exécuter la commande
    ssh_channel channel = ssh_channel_new(sess->session);
    if (!channel) {
        json_object_put(root);
        *response = strdup("{\"error\":\"Impossible de créer le canal\"}");
        return RESP_SSH_ERROR;
    }

    if (ssh_channel_open_session(channel) != SSH_OK) {
        ssh_channel_free(channel);
        json_object_put(root);
        *response = strdup("{\"error\":\"Impossible d'ouvrir le canal\"}");
        return RESP_SSH_ERROR;
    }

    if (ssh_channel_request_exec(channel, command) != SSH_OK) {
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        json_object_put(root);
        *response = strdup("{\"error\":\"Impossible d'exécuter la commande\"}");
        return RESP_SSH_ERROR;
    }

    // Utiliser les buffers Rust pour une gestion mémoire sécurisée (optimisé)
    void *stdout_buffer = rust_buffer_new(8192);  // Capacité initiale plus grande
    void *stderr_buffer = rust_buffer_new(4096);
    
    if (!stdout_buffer || !stderr_buffer) {
        if (stdout_buffer) rust_buffer_free(stdout_buffer);
        if (stderr_buffer) rust_buffer_free(stderr_buffer);
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        json_object_put(root);
        *response = strdup("{\"error\":\"Erreur d'allocation mémoire\"}");
        return RESP_ERROR;
    }
    
    // Lire stdout (optimisé : pas besoin de null terminator)
    int nbytes;
    do {
        char buf[4096];
        nbytes = ssh_channel_read(channel, buf, sizeof(buf), 0);
        if (nbytes > 0) {
            if (rust_buffer_append(stdout_buffer, buf, nbytes) != 0) {
                rust_buffer_free(stdout_buffer);
                rust_buffer_free(stderr_buffer);
                ssh_channel_close(channel);
                ssh_channel_free(channel);
                json_object_put(root);
                *response = strdup("{\"error\":\"Erreur lors de la lecture\"}");
                return RESP_ERROR;
            }
        }
    } while (nbytes > 0);
    
    // Lire stderr
    int stderr_bytes;
    do {
        char stderr_buf[4096];
        stderr_bytes = ssh_channel_read(channel, stderr_buf, sizeof(stderr_buf), 1);
        if (stderr_bytes > 0) {
            rust_buffer_append(stderr_buffer, stderr_buf, stderr_bytes);
        }
    } while (stderr_bytes > 0);
    
    int exit_status = ssh_channel_get_exit_status(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);

    // Obtenir les données des buffers
    size_t stdout_len = rust_buffer_len(stdout_buffer);
    size_t stderr_len = rust_buffer_len(stderr_buffer);
    const char *stdout_data = (const char*)rust_buffer_data(stdout_buffer);
    const char *stderr_data = (const char*)rust_buffer_data(stderr_buffer);

    // Allouer les buffers d'échappement avec taille estimée (évite les réallocations)
    // Estimation : la plupart des sorties n'ont pas besoin d'échappement complet
    size_t escaped_stdout_size = stdout_len + (stdout_len / 4) + 256;  // 25% overhead au lieu de 100%
    size_t escaped_stderr_size = stderr_len + (stderr_len / 4) + 256;
    char *escaped_stdout = malloc(escaped_stdout_size);
    char *escaped_stderr = NULL;
    
    if (!escaped_stdout) {
        rust_buffer_free(stdout_buffer);
        rust_buffer_free(stderr_buffer);
        json_object_put(root);
        *response = strdup("{\"error\":\"Erreur d'allocation mémoire\"}");
        return RESP_ERROR;
    }
    
    int escaped_stdout_len = rust_escape_json(stdout_data, escaped_stdout, escaped_stdout_size);
    if (escaped_stdout_len < 0) {
        // Buffer trop petit, réessayer avec taille maximale
        free(escaped_stdout);
        escaped_stdout_size = stdout_len * 2 + 1;
        escaped_stdout = malloc(escaped_stdout_size);
        if (!escaped_stdout) {
            rust_buffer_free(stdout_buffer);
            rust_buffer_free(stderr_buffer);
            json_object_put(root);
            *response = strdup("{\"error\":\"Erreur d'allocation mémoire\"}");
            return RESP_ERROR;
        }
        escaped_stdout_len = rust_escape_json(stdout_data, escaped_stdout, escaped_stdout_size);
        if (escaped_stdout_len < 0) {
            free(escaped_stdout);
            rust_buffer_free(stdout_buffer);
            rust_buffer_free(stderr_buffer);
            json_object_put(root);
            *response = strdup("{\"error\":\"Erreur lors de l'échappement JSON\"}");
            return RESP_ERROR;
        }
    }
    
    if (stderr_len > 0) {
        escaped_stderr = malloc(escaped_stderr_size);
        if (escaped_stderr) {
            int escaped_stderr_len = rust_escape_json(stderr_data, escaped_stderr, escaped_stderr_size);
            if (escaped_stderr_len < 0) {
                // Buffer trop petit, réessayer
                free(escaped_stderr);
                escaped_stderr_size = stderr_len * 2 + 1;
                escaped_stderr = malloc(escaped_stderr_size);
                if (escaped_stderr) {
                    rust_escape_json(stderr_data, escaped_stderr, escaped_stderr_size);
                }
            }
        }
    }

    size_t response_size = escaped_stdout_len + (escaped_stderr ? strlen(escaped_stderr) : 0) + 128;
    char *response_json = malloc(response_size);
    if (!response_json) {
        free(escaped_stdout);
        if (escaped_stderr) free(escaped_stderr);
        rust_buffer_free(stdout_buffer);
        rust_buffer_free(stderr_buffer);
        json_object_put(root);
        *response = strdup("{\"error\":\"Erreur d'allocation mémoire\"}");
        return RESP_ERROR;
    }
    
    if (escaped_stderr && stderr_len > 0) {
        snprintf(response_json, response_size,
                "{\"output\":\"%s\",\"stderr\":\"%s\",\"exit_code\":%d,\"bytes_read\":%zu}",
                escaped_stdout, escaped_stderr, exit_status, stdout_len);
    } else {
        snprintf(response_json, response_size,
                "{\"output\":\"%s\",\"exit_code\":%d,\"bytes_read\":%zu}",
                escaped_stdout, exit_status, stdout_len);
    }
    
    free(escaped_stdout);
    if (escaped_stderr) free(escaped_stderr);
    rust_buffer_free(stdout_buffer);
    rust_buffer_free(stderr_buffer);
    *response = response_json;

    json_object_put(root);
    return RESP_OK;
}

/**
 * Gérer le statut SSH
 */
response_code_t handle_ssh_status(const char *json_data, char **response) {
    if (!json_data || !response) {
        if (response) *response = strdup("{\"error\":\"Paramètres invalides\"}");
        return RESP_ERROR;
    }
    
    json_object *root;
    JSON_PARSE_OR_RETURN(json_data, root, "JSON invalide");

    const char *session_id;
    JSON_GET_STRING_OR_RETURN(root, "session_id", session_id, "session_id requis");
    ssh_session_t *sess = find_session(session_id);

    if (!sess) {
        *response = strdup("{\"status\":\"not_found\"}");
    } else if (sess->connected) {
        char response_json[256];
        snprintf(response_json, sizeof(response_json),
                "{\"status\":\"connected\",\"created_at\":%ld}",
                sess->created_at);
        *response = strdup(response_json);
    } else {
        *response = strdup("{\"status\":\"disconnected\"}");
    }

    json_object_put(root);
    return RESP_OK;
}

/**
 * Lister toutes les sessions
 */
response_code_t handle_list_sessions(char **response) {
    pthread_mutex_lock(&sessions_mutex);
    void *json_buffer = rust_buffer_new(256);
    if (!json_buffer) {
        pthread_mutex_unlock(&sessions_mutex);
        *response = strdup("{\"error\":\"Erreur d'allocation mémoire\"}");
        return RESP_ERROR;
    }
    
    rust_buffer_append(json_buffer, "{\"sessions\":[", 13);
    int count = 0;
    for (int i = 0; i < session_count; i++) {
        if (sessions[i].connected) {
            if (count > 0) rust_buffer_append(json_buffer, ",", 1);
            char session_json[128];
            int n = snprintf(session_json, sizeof(session_json),
                    "{\"id\":\"%s\",\"status\":\"connected\",\"created_at\":%ld}",
                    sessions[i].session_id, sessions[i].created_at);
            if (n > 0) rust_buffer_append(json_buffer, session_json, n);
            count++;
        }
    }
    
    char count_str[32];
    int count_len = snprintf(count_str, sizeof(count_str), "],\"count\":%d}", count);
    rust_buffer_append(json_buffer, count_str, count_len);
    
    size_t json_len = rust_buffer_len(json_buffer);
    const char *json_data = (const char*)rust_buffer_data(json_buffer);
    char *json = malloc(json_len + 1);
    if (!json) {
        rust_buffer_free(json_buffer);
        pthread_mutex_unlock(&sessions_mutex);
        *response = strdup("{\"error\":\"Erreur d'allocation mémoire\"}");
        return RESP_ERROR;
    }
    
    memcpy(json, json_data, json_len);
    json[json_len] = '\0';
    
    rust_buffer_free(json_buffer);
    pthread_mutex_unlock(&sessions_mutex);
    *response = json;
    return RESP_OK;
}

