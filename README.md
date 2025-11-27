# Krown Agent

> Agent SSH daemon en C avec gestion mémoire sécurisée en Rust

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Rust](https://img.shields.io/badge/rust-1.70+-orange.svg)](https://www.rust-lang.org/)
[![C](https://img.shields.io/badge/C-C11-blue.svg)](https://en.wikipedia.org/wiki/C11_(C_standard_revision))

## 🚀 Démarrage Rapide

### Installation

```bash
# Installer les dépendances
make deps

# Compiler
make

# Installer
make install
```

### Installation Automatique (Recommandé)

```bash
# Linux - Installation locale
chmod +x scripts/install.sh
./scripts/install.sh

# Linux - Installation sur machine distante
chmod +x scripts/deploy-remote.sh
./scripts/deploy-remote.sh user@host

# Windows (PowerShell)
.\scripts\install.ps1
```

Le script installe automatiquement Docker si nécessaire, construit l'image et démarre le conteneur avec redémarrage automatique.

**Le conteneur redémarre automatiquement au boot de la machine grâce à `--restart=always`.**

Voir [docs/INSTALL.md](docs/INSTALL.md) pour plus de détails.

### Docker Manuel

```bash
# Avec docker-compose
docker-compose -f config/docker-compose.yml up -d

# Ou avec docker run
docker run -d --name krown-agent --restart=always \
  -v /run/krown:/run/krown -v /tmp:/tmp \
  -e SOCKET_PATH=/run/krown/krown-agent.sock \
  krown-agent
```

## 📚 Documentation

Consultez **[docs/DOCUMENTATION.md](docs/DOCUMENTATION.md)** pour la documentation complète.

La documentation inclut :
- ✅ Guide d'installation et compilation
- ✅ Déploiement Docker avec démarrage automatique
- ✅ Gestion mémoire Rust (FFI)
- ✅ Configuration et utilisation
- ✅ Dépannage

## 📁 Structure du Projet

```
agent/
├── src/              # Code source C
├── src-rust/         # Code source Rust
├── docs/             # Documentation (DOCUMENTATION.md, INSTALL.md, CONTRIBUTING.md)
├── config/           # Configuration (systemd, docker-compose)
├── scripts/          # Scripts (installation, déploiement, démarrage)
├── bin/              # Binaires compilés (généré)
├── build/            # Fichiers objets (généré)
├── target/           # Artifacts Rust (généré)
│
├── README.md         # Documentation principale
├── Cargo.toml        # Configuration Rust
├── Makefile          # Build system
├── Dockerfile        # Image Docker
├── .gitignore        # Fichiers ignorés par Git
└── .dockerignore     # Fichiers ignorés par Docker
```

## 🏗️ Architecture

- **C**: Logique métier (SSH, sockets, requêtes)
- **Rust**: Gestion mémoire sécurisée (buffers, allocations)
- **FFI**: Interface C/Rust pour la communication

## 📋 Fonctionnalités

- ✅ Gestion mémoire sécurisée avec Rust
- ✅ Support complet SSH (mot de passe, clés privées)
- ✅ Socket Unix pour communication locale
- ✅ Multi-threading pour requêtes concurrentes
- ✅ Démarrage automatique avec Docker
- ✅ Service systemd intégré

## 🔧 Commandes Utiles

```bash
make help          # Afficher l'aide
make deps          # Installer les dépendances
make clean         # Nettoyer les fichiers de build
make check         # Vérifier l'installation
```

## 📝 Licence

[À définir]

