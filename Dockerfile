# Dockerfile pour krown-agent (Daemon C + Rust)
FROM debian:bookworm-slim

# Installer les dépendances système (optimisé)
RUN apt-get update && apt-get install -y --no-install-recommends \
    libssh-dev \
    libjson-c-dev \
    build-essential \
    curl \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# Installer Rust
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y && \
    . $HOME/.cargo/env && \
    rustc --version

# Créer le répertoire de travail
WORKDIR /app

# Copier les fichiers sources (C et Rust)
COPY src/ ./src/
COPY src-rust/ ./src-rust/
COPY Cargo.toml ./
COPY Makefile ./
COPY .gitignore ./
COPY scripts/ ./scripts/
COPY config/ ./config/

# Vérifier que les fichiers nécessaires sont présents
RUN ls -la src/ && \
    test -f Makefile && echo "✓ Makefile présent" && \
    test -f src/main.c && echo "✓ main.c présent" && \
    test -f src/ssh_handler.c && echo "✓ ssh_handler.c présent" && \
    test -f src-rust/lib.rs && echo "✓ lib.rs présent" && \
    test -f Cargo.toml && echo "✓ Cargo.toml présent" || \
    (echo "✗ Fichiers manquants" && exit 1)

# Compiler l'agent (C + Rust)
RUN . $HOME/.cargo/env && \
    make clean && \
    make && \
    test -f bin/krown-agent && echo "✓ Compilation réussie" || \
    (echo "✗ Échec de la compilation" && exit 1)

# Installer l'agent dans /usr/local/bin
RUN cp bin/krown-agent /usr/local/bin/krown-agent && \
    chmod +x /usr/local/bin/krown-agent && \
    echo "✓ Agent installé dans /usr/local/bin"

# Créer un utilisateur non-root
RUN useradd -r -s /bin/false krown && \
    chown -R krown:krown /app

# Créer le répertoire pour les logs
RUN mkdir -p /var/log/krown && \
    chown krown:krown /var/log/krown

# Créer le répertoire pour le socket
RUN mkdir -p /run/krown && \
    chown krown:krown /run/krown


# Script d'initialisation Docker (optimisé)
RUN echo '#!/bin/bash' > /docker-entrypoint.sh && \
    echo 'set -e' >> /docker-entrypoint.sh && \
    echo 'SOCKET="${SOCKET_PATH:-/run/krown/krown-agent.sock}"' >> /docker-entrypoint.sh && \
    echo 'mkdir -p "$(dirname "$SOCKET")"' >> /docker-entrypoint.sh && \
    echo 'chmod 755 "$(dirname "$SOCKET")"' >> /docker-entrypoint.sh && \
    echo 'exec /usr/local/bin/krown-agent "$SOCKET"' >> /docker-entrypoint.sh && \
    chmod +x /docker-entrypoint.sh

# Exposer le socket
VOLUME ["/run/krown", "/tmp"]

# Utiliser root pour systemd (nécessaire pour systemd dans Docker)
USER root

# Point d'entrée
ENTRYPOINT ["/docker-entrypoint.sh"]
CMD []

# Politique de redémarrage (sera utilisé avec docker run --restart=always)
# Label pour docker-compose
LABEL restart.policy="always"

