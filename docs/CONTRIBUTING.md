# Guide de Contribution

## Structure du Code

### Code C (`src/`)
- `main.c`: Point d'entrée, boucle principale
- `agent.h`: Définitions communes (protocole, structures)
- `ssh_handler.c/h`: Gestion des connexions SSH
- `socket_server.c/h`: Serveur socket Unix
- `request_handler.c/h`: Traitement des requêtes client
- `memory.h`: Interface FFI Rust (copie de `src-rust/memory.h`)

### Code Rust (`src-rust/`)
- `lib.rs`: Bibliothèque de gestion mémoire sécurisée
- `memory.h`: En-têtes C pour l'interface FFI

## Conventions de Code

### C
- Style: K&R avec 4 espaces
- Noms: `snake_case` pour fonctions et variables
- Commentaires: En français pour la documentation

### Rust
- Style: Standard Rust (rustfmt)
- Noms: `snake_case` pour fonctions, `PascalCase` pour types
- Documentation: En français avec `///`

## Compilation

```bash
# Développement
make clean
make

# Release
make clean
CARGO_PROFILE=release make
```

## Tests

```bash
# Vérifier la compilation
make check

# Tester le binaire
./bin/krown-agent /tmp/test.sock
```

## Soumission de Modifications

1. Créer une branche depuis `main`
2. Faire les modifications
3. Tester la compilation et l'exécution
4. Créer une pull request

