# Documentation Complète - Krown Agent

> Agent SSH daemon en C avec gestion mémoire sécurisée en Rust

---

## Table des Matières

1. [Vue d'ensemble](#vue-densemble)
2. [Structure du Projet](#structure-du-projet)
3. [Installation et Compilation](#installation-et-compilation)
4. [Déploiement Docker](#déploiement-docker)
5. [Gestion Mémoire Rust](#gestion-mémoire-rust)
6. [Configuration](#configuration)
7. [Utilisation](#utilisation)
8. [Dépannage](#dépannage)

---

## Vue d'ensemble

Krown Agent est un daemon SSH écrit en C qui utilise Rust pour la gestion mémoire sécurisée. Il écoute sur un socket Unix local et répond aux commandes du backend Node.js pour gérer les connexions SSH.

### Caractéristiques

- ✅ Gestion mémoire sécurisée avec Rust
- ✅ Support complet SSH (mot de passe, clés privées)
- ✅ Socket Unix pour communication locale
- ✅ Multi-threading pour requêtes concurrentes
- ✅ Démarrage automatique avec Docker
- ✅ Service systemd intégré

### Architecture

- **C**: Logique métier (SSH, sockets, requêtes)
- **Rust**: Gestion mémoire sécurisée (buffers, allocations)
- **FFI**: Interface C/Rust pour la communication

---

## Structure du Projet

```
agent/
│
├── 📁 src/                      # Code source C
│   ├── main.c                  # Point d'entrée principal
│   ├── agent.h                 # En-têtes principaux (protocole, structures)
│   ├── memory.h                # Interface FFI Rust (copie de src-rust/)
│   ├── ssh_handler.c/h         # Gestionnaire SSH (libssh)
│   ├── socket_server.c/h       # Serveur socket Unix
│   └── request_handler.c/h     # Gestionnaire de requêtes client
│
├── 📁 src-rust/                # Code source Rust
│   ├── lib.rs                  # Bibliothèque de gestion mémoire sécurisée
│   └── memory.h                # En-têtes C pour l'interface FFI
│
├── 📁 bin/                     # Binaires compilés (généré)
│   └── krown-agent            # Exécutable final
│
├── 📁 build/                   # Fichiers objets (généré)
│   └── *.o                    # Fichiers objets C
│
├── 📁 target/                  # Artifacts Rust (généré)
│   └── release/
│       └── libkrown_memory.a  # Bibliothèque statique Rust
│
├── 📄 Cargo.toml               # Configuration Rust
├── 📄 Makefile                 # Build system (C + Rust)
├── 📄 Dockerfile               # Image Docker
├── 📄 docker-compose.yml       # Configuration Docker Compose
├── 📄 .dockerignore            # Fichiers ignorés par Docker
├── 📄 .gitignore               # Fichiers ignorés par Git
│
├── 📄 krown-agent.service      # Service systemd
├── 📄 start-agent.sh           # Script de démarrage
│
└── 📄 Documentation/
    ├── DOCUMENTATION.md        # Ce fichier (documentation complète)
    └── CONTRIBUTING.md         # Guide de contribution
```

### Flux de Compilation

```
1. Rust (Cargo)
   src-rust/lib.rs → target/release/libkrown_memory.a

2. C (GCC)
   src/*.c → build/*.o

3. Linkage
   build/*.o + libkrown_memory.a → bin/krown-agent
```

### Flux de Données

```
Client (Node.js)
    ↓ (Socket Unix)
socket_server.c
    ↓
request_handler.c
    ↓
ssh_handler.c → Rust (gestion mémoire)
    ↓
libssh → Serveur SSH distant
```

---

## Installation et Compilation

### Prérequis

- **Système**: Linux (Debian/Ubuntu recommandé)
- **Dépendances système**:
  - `libssh-dev`
  - `libjson-c-dev`
  - `build-essential`
  - `curl` (pour installer Rust)
- **Rust**: Installé automatiquement via le Makefile

### Installation des Dépendances

```bash
# Installer automatiquement toutes les dépendances
make deps
```

Ou manuellement :

```bash
# Ubuntu/Debian
sudo apt-get update
sudo apt-get install -y libssh-dev libjson-c-dev build-essential

# Installer Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source $HOME/.cargo/env
```

### Compilation

```bash
# Compiler tout (C + Rust)
make

# Nettoyer et recompiler
make clean && make

# Vérifier l'installation
make check
```

### Installation du Binaire

```bash
# Installer dans /usr/local/bin
make install

# Installer le service systemd
make install-service

# Activer le service au démarrage
sudo systemctl enable krown-agent.service
sudo systemctl start krown-agent.service
```

### Options de Compilation

Le Makefile supporte plusieurs cibles :

- `all` : Compile tout (défaut)
- `clean` : Nettoie les fichiers de build
- `install` : Installe le binaire dans /usr/local/bin
- `install-service` : Installe le service systemd
- `deps` : Installe les dépendances
- `check` : Vérifie l'installation
- `help` : Affiche l'aide

---

## Déploiement Docker

### 🚀 Démarrage Automatique

#### Option 1: Docker Compose (Recommandé)

```bash
# Construire et démarrer
docker-compose up -d

# Vérifier le statut
docker-compose ps

# Voir les logs
docker-compose logs -f krown-agent

# Arrêter
docker-compose down
```

Le conteneur redémarrera automatiquement :
- Au redémarrage de la machine (grâce à `restart: always`)
- En cas de crash de l'agent
- Après un redémarrage de Docker

#### Option 2: Docker Run

```bash
# Construire l'image
docker build -t krown-agent .

# Démarrer avec redémarrage automatique
docker run -d \
  --name krown-agent \
  --restart=always \
  --privileged \
  -v /run/krown:/run/krown \
  -v /tmp:/tmp \
  -e SOCKET_PATH=/run/krown/krown-agent.sock \
  krown-agent
```

#### Option 3: Systemd dans le Conteneur

Si vous utilisez systemd dans Docker (nécessite `--privileged`) :

```bash
docker run -d \
  --name krown-agent \
  --restart=always \
  --privileged \
  -v /run/krown:/run/krown \
  -v /sys/fs/cgroup:/sys/fs/cgroup:ro \
  krown-agent
```

Le service systemd démarrera automatiquement l'agent au démarrage du conteneur.

### Configuration Docker

#### Variables d'Environnement

- `SOCKET_PATH`: Chemin du socket Unix (défaut: `/run/krown/krown-agent.sock`)
- `RUST_LOG`: Niveau de log Rust (défaut: `info`)

#### Volumes

- `/run/krown`: Répertoire pour le socket Unix
- `/tmp`: Répertoire temporaire
- `/var/log/krown`: Logs de l'agent

### Vérification

#### Vérifier que l'agent fonctionne

```bash
# Vérifier que le socket existe
test -S /run/krown/krown-agent.sock && echo "✓ Socket actif" || echo "✗ Socket introuvable"

# Vérifier les logs
docker logs krown-agent

# Vérifier le processus
docker exec krown-agent ps aux | grep krown-agent
```

#### Test de connexion

```bash
# Depuis l'hôte (si le socket est monté)
socat - UNIX-CONNECT:/run/krown/krown-agent.sock
```

### Redémarrage Automatique

L'agent redémarre automatiquement dans les cas suivants :

1. **Redémarrage de la machine** : Grâce à `restart=always` dans Docker
2. **Crash de l'agent** : Le service systemd ou Docker le relance
3. **Redémarrage de Docker** : Le conteneur redémarre automatiquement

#### Configuration systemd sur l'hôte (Optionnel)

Pour démarrer automatiquement le conteneur au boot de la machine :

```bash
# Créer un service systemd pour Docker
sudo nano /etc/systemd/system/krown-agent-docker.service
```

```ini
[Unit]
Description=Krown Agent Docker Container
Requires=docker.service
After=docker.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/usr/bin/docker start krown-agent
ExecStop=/usr/bin/docker stop krown-agent
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

```bash
# Activer le service
sudo systemctl enable krown-agent-docker.service
sudo systemctl start krown-agent-docker.service
```

### Dépannage Docker

#### L'agent ne démarre pas

```bash
# Vérifier les logs
docker logs krown-agent

# Vérifier les permissions du socket
ls -la /run/krown/

# Redémarrer le conteneur
docker restart krown-agent
```

#### Le socket n'est pas accessible

```bash
# Vérifier que le volume est monté
docker inspect krown-agent | grep Mounts

# Vérifier les permissions
docker exec krown-agent ls -la /run/krown/
```

#### Problèmes de compilation Rust

```bash
# Reconstruire sans cache
docker build --no-cache -t krown-agent .
```

### Notes Docker

- Le conteneur nécessite `--privileged` pour systemd
- Le socket doit être monté comme volume pour être accessible depuis l'hôte
- Les logs sont disponibles via `docker logs` ou dans `/var/log/krown`

---

## Gestion Mémoire Rust

### Intégration Rust pour la Gestion Mémoire

Ce projet utilise **Rust** pour la gestion mémoire sécurisée, tout en conservant le code C existant.

### Architecture

- **Code C** : Logique métier principale (SSH, sockets, requêtes)
- **Code Rust** : Gestion mémoire sécurisée (buffers, allocations, échappement JSON)

### Structure

```
agent/
├── src/              # Code C (inchangé)
├── src-rust/        # Code Rust
│   ├── lib.rs       # Bibliothèque Rust avec FFI
│   └── memory.h     # En-têtes C pour l'interface Rust
├── Cargo.toml       # Configuration Rust
└── Makefile         # Compilation hybride C+Rust
```

### Compilation

#### Prérequis

```bash
# Installer les dépendances
make deps
```

#### Compiler

```bash
# Compile automatiquement Rust puis C
make
```

Le Makefile :
1. Compile la bibliothèque Rust (`cargo build --release`)
2. Compile le code C avec les en-têtes Rust
3. Lie tout ensemble

### Fonctionnalités Rust

#### Buffers Sécurisés

```c
// Créer un buffer
void* buffer = rust_buffer_new(4096);

// Ajouter des données
rust_buffer_append(buffer, data, data_len);

// Obtenir les données
const void* data = rust_buffer_data(buffer);
size_t len = rust_buffer_len(buffer);

// Libérer
rust_buffer_free(buffer);
```

#### Gestion Mémoire

```c
// Allocation sécurisée
void* ptr = rust_malloc(size);

// Réallocation
ptr = rust_realloc(ptr, old_size, new_size);

// Libération
rust_free(ptr, size);
```

#### Échappement JSON

```c
char output[1024];
rust_escape_json(input_string, output, sizeof(output));
```

### Avantages

1. **Sécurité mémoire** : Rust garantit la sécurité mémoire à la compilation
2. **Pas de fuites** : Gestion automatique de la mémoire
3. **Performance** : Pas de surcoût, même performance que C natif
4. **Compatibilité** : Interface C standard, aucun changement dans le code C existant

### Utilisation dans le Code

Le code C utilise maintenant Rust pour :
- Lecture des sorties SSH (`handle_ssh_execute`)
- Construction de JSON (`handle_list_sessions`)
- Échappement de chaînes JSON

Tout le reste du code C reste inchangé.

### Optimisations

- **Buffers dynamiques** : Allocation intelligente avec croissance exponentielle (1.5x)
- **Échappement JSON optimisé** : Détection préalable si échappement nécessaire
- **Zero-copy quand possible** : Réduction des copies mémoire
- **LTO (Link-Time Optimization)** : Optimisations à la liaison

---

## Configuration

### Variables d'Environnement

- `SOCKET_PATH`: Chemin du socket Unix (défaut: `/tmp/krown-agent.sock`)
- `RUST_LOG`: Niveau de log Rust (défaut: `info`)

### Service Systemd

Le service systemd est configuré pour :

- **Démarrage automatique** au boot
- **Redémarrage automatique** en cas de crash
- **Logs** via journald
- **Sécurité** : Utilisateur dédié, restrictions de permissions

#### Installation

```bash
# Installer le service
sudo cp krown-agent.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable krown-agent.service
sudo systemctl start krown-agent.service
```

#### Commandes Utiles

```bash
# Démarrer
sudo systemctl start krown-agent.service

# Arrêter
sudo systemctl stop krown-agent.service

# Redémarrer
sudo systemctl restart krown-agent.service

# Voir le statut
sudo systemctl status krown-agent.service

# Voir les logs
sudo journalctl -u krown-agent.service -f
```

### Configuration du Socket

Le socket Unix est créé automatiquement au démarrage. Par défaut :
- Chemin : `/run/krown/krown-agent.sock` (ou `/tmp/krown-agent.sock`)
- Permissions : 0666 (modifiable dans le code)

---

## Utilisation

### Démarrage Manuel

```bash
# Avec chemin par défaut
./bin/krown-agent

# Avec chemin personnalisé
./bin/krown-agent /custom/path/krown-agent.sock

# Avec variable d'environnement
SOCKET_PATH=/custom/path/krown-agent.sock ./bin/krown-agent
```

### Protocole de Communication

L'agent utilise un protocole binaire sur socket Unix :

#### En-tête de Commande
```
[version: uint32] [type: uint32] [data_len: uint32] [data: bytes]
```

#### Types de Commandes
- `CMD_PING = 1` : Test de connexion
- `CMD_SSH_CONNECT = 2` : Connexion SSH
- `CMD_SSH_DISCONNECT = 3` : Déconnexion SSH
- `CMD_SSH_EXECUTE = 4` : Exécution de commande
- `CMD_SSH_STATUS = 5` : Statut de session
- `CMD_LIST_SESSIONS = 6` : Liste des sessions

#### Codes de Réponse
- `RESP_OK = 0` : Succès
- `RESP_ERROR = 1` : Erreur générale
- `RESP_INVALID_CMD = 2` : Commande invalide
- `RESP_SSH_ERROR = 3` : Erreur SSH

### Exemple d'Utilisation (Node.js)

```javascript
const net = require('net');
const socketPath = '/run/krown/krown-agent.sock';

// Connexion
const client = net.createConnection(socketPath);

// Envoyer une commande PING
const command = {
  version: 1,
  type: 1, // CMD_PING
  data: JSON.stringify({})
};

// ... (implémentation complète)
```

---

## Dépannage

### Problèmes de Compilation

#### Erreur Rust non trouvé
```bash
# Installer Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source $HOME/.cargo/env
```

#### Erreur libssh non trouvée
```bash
# Installer libssh
sudo apt-get install libssh-dev
```

#### Erreur de linkage
```bash
# Vérifier que la bibliothèque Rust est compilée
ls -la target/release/libkrown_memory.a

# Recompiler Rust
cargo build --release
```

### Problèmes d'Exécution

#### Socket déjà utilisé
```bash
# Trouver le processus utilisant le socket
lsof /run/krown/krown-agent.sock

# Tuer le processus ou changer le chemin du socket
```

#### Permissions insuffisantes
```bash
# Vérifier les permissions du socket
ls -la /run/krown/krown-agent.sock

# Ajuster les permissions si nécessaire
chmod 666 /run/krown/krown-agent.sock
```

#### L'agent ne démarre pas
```bash
# Vérifier les logs
journalctl -u krown-agent.service -n 50

# Vérifier les dépendances
ldd /usr/local/bin/krown-agent
```

### Problèmes Docker

Voir la section [Dépannage Docker](#dépannage-docker) ci-dessus.

### Problèmes SSH

#### Authentification échouée
- Vérifier les identifiants (username/password)
- Vérifier que la clé publique est dans `~/.ssh/authorized_keys`
- Vérifier les permissions : `~/.ssh` (700), `authorized_keys` (600)

#### Connexion timeout
- Vérifier la connectivité réseau
- Vérifier le port SSH (défaut: 22)
- Vérifier le firewall

---

## Support

Pour plus d'informations ou pour signaler un problème, consultez le [Guide de Contribution](CONTRIBUTING.md).

---

**Version**: 1.0  
**Dernière mise à jour**: 2024

