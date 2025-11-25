# Structure du Projet Krown Agent

## 📂 Organisation

```
agent/
│
├── 📁 src/                    # Code source C
│   ├── main.c
│   ├── agent.h
│   ├── memory.h
│   ├── ssh_handler.c/h
│   ├── socket_server.c/h
│   └── request_handler.c/h
│
├── 📁 src-rust/              # Code source Rust
│   ├── lib.rs
│   └── memory.h
│
├── 📁 docs/                  # Documentation
│   ├── DOCUMENTATION.md      # Documentation complète
│   ├── CONTRIBUTING.md       # Guide de contribution
│   └── README.md
│
├── 📁 config/                 # Configuration
│   ├── krown-agent.service   # Service systemd
│   ├── docker-compose.yml    # Docker Compose
│   └── README.md
│
├── 📁 scripts/               # Scripts utilitaires
│   ├── start-agent.sh        # Script de démarrage
│   └── README.md
│
├── 📁 bin/                   # Binaires compilés (généré)
│   └── krown-agent
│
├── 📁 build/                 # Fichiers objets (généré)
│   └── *.o
│
├── 📁 target/                # Artifacts Rust (généré)
│   └── release/
│       └── libkrown_memory.a
│
├── 📄 Cargo.toml              # Configuration Rust
├── 📄 Makefile               # Build system
├── 📄 Dockerfile             # Image Docker
├── 📄 README.md              # Documentation principale
├── 📄 .gitignore             # Fichiers ignorés par Git
└── 📄 .dockerignore          # Fichiers ignorés par Docker
```

## 🎯 Répertoires Principaux

### `src/` - Code C
Contient tout le code source C :
- Logique métier (SSH, sockets)
- Gestion des requêtes
- Interface avec Rust

### `src-rust/` - Code Rust
Bibliothèque Rust pour la gestion mémoire :
- Buffers sécurisés
- Allocations mémoire
- Échappement JSON

### `docs/` - Documentation
Toute la documentation du projet :
- Documentation complète
- Guide de contribution

### `config/` - Configuration
Fichiers de configuration :
- Service systemd
- Docker Compose

### `scripts/` - Scripts
Scripts utilitaires :
- Démarrage de l'agent
- Autres utilitaires

## 🔄 Flux de Compilation

```
1. Rust (Cargo)
   src-rust/lib.rs → target/release/libkrown_memory.a

2. C (GCC)
   src/*.c → build/*.o

3. Linkage
   build/*.o + libkrown_memory.a → bin/krown-agent
```

## 📝 Notes

- Les répertoires `bin/`, `build/`, `target/` sont générés automatiquement
- Ne pas commiter ces répertoires dans Git
- La documentation est centralisée dans `docs/`
- Les configurations sont dans `config/`
- Les scripts sont dans `scripts/`

