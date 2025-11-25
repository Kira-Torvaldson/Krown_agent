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

### Docker

```bash
# Démarrer avec docker-compose
docker-compose up -d
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
├── docs/             # Documentation
├── config/            # Configuration (systemd, docker-compose)
├── scripts/          # Scripts utilitaires
├── bin/              # Binaires (généré)
├── build/            # Objets (généré)
└── target/           # Rust artifacts (généré)
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

