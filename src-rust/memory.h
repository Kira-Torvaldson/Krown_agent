/**
 * Krown Memory Manager - En-têtes C pour l'interface Rust
 * 
 * Ces fonctions permettent au code C d'utiliser la gestion mémoire
 * sécurisée de Rust via FFI.
 */

#ifndef KROWN_MEMORY_H
#define KROWN_MEMORY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Gestion de buffers sécurisés
// ============================================================================

/**
 * Créer un nouveau buffer sécurisé
 * @param initial_capacity Capacité initiale en bytes
 * @return Pointeur opaque vers le buffer, ou NULL en cas d'erreur
 */
void* rust_buffer_new(size_t initial_capacity);

/**
 * Ajouter des données au buffer
 * @param buffer_ptr Pointeur vers le buffer (retourné par rust_buffer_new)
 * @param data Pointeur vers les données à ajouter
 * @param data_len Longueur des données en bytes
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int rust_buffer_append(void* buffer_ptr, const void* data, size_t data_len);

/**
 * Obtenir la longueur actuelle du buffer
 * @param buffer_ptr Pointeur vers le buffer
 * @return Longueur en bytes, ou 0 si le pointeur est NULL
 */
size_t rust_buffer_len(const void* buffer_ptr);

/**
 * Obtenir un pointeur vers les données du buffer (read-only)
 * @param buffer_ptr Pointeur vers le buffer
 * @return Pointeur vers les données, ou NULL si le pointeur est NULL
 */
const void* rust_buffer_data(const void* buffer_ptr);

/**
 * Libérer un buffer
 * @param buffer_ptr Pointeur vers le buffer à libérer
 */
void rust_buffer_free(void* buffer_ptr);

// ============================================================================
// Gestion mémoire générale
// ============================================================================

/**
 * Allouer de la mémoire de manière sécurisée
 * @param size Taille à allouer en bytes
 * @return Pointeur vers la mémoire allouée, ou NULL en cas d'erreur
 */
void* rust_malloc(size_t size);

/**
 * Libérer de la mémoire allouée par rust_malloc
 * @param ptr Pointeur vers la mémoire à libérer
 * @param size Taille de la mémoire allouée (pour vérification)
 */
void rust_free(void* ptr, size_t size);

/**
 * Réallouer de la mémoire
 * @param ptr Pointeur vers l'ancienne mémoire (peut être NULL)
 * @param old_size Ancienne taille
 * @param new_size Nouvelle taille
 * @return Pointeur vers la nouvelle mémoire, ou NULL en cas d'erreur
 */
void* rust_realloc(void* ptr, size_t old_size, size_t new_size);

// ============================================================================
// Utilitaires
// ============================================================================

/**
 * Échapper une chaîne pour JSON
 * @param input Chaîne à échapper (null-terminated)
 * @param output Buffer de sortie
 * @param output_size Taille du buffer de sortie
 * @return Longueur de la chaîne échappée, ou -1 en cas d'erreur
 */
int rust_escape_json(const char* input, char* output, size_t output_size);

/**
 * Copier de la mémoire de manière sécurisée
 * @param dest Destination
 * @param src Source
 * @param n Nombre de bytes à copier
 * @return 0 en cas de succès, -1 en cas d'erreur
 */
int rust_memcpy(void* dest, const void* src, size_t n);

#ifdef __cplusplus
}
#endif

#endif // KROWN_MEMORY_H

