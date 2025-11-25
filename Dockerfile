# Dockerfile pour krown-agent (Daemon C + Rust)
FROM debian:bookworm-slim

# Installer les dépendances système
RUN apt-get update && apt-get install -y \
    libssh-dev \
    libjson-c-dev \
    build-essential \
    curl \
    ca-certificates \
    systemd \
    systemd-sysv \
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

# Copier et configurer le service systemd
COPY config/krown-agent.service /etc/systemd/system/krown-agent.service
RUN systemctl enable krown-agent.service && \
    echo "✓ Service systemd configuré"

# Copier et installer le script de démarrage
COPY scripts/start-agent.sh /usr/local/bin/start-agent.sh
RUN chmod +x /usr/local/bin/start-agent.sh

# Script d'initialisation Docker
RUN echo '#!/bin/bash' > /docker-entrypoint.sh && \
    echo 'set -e' >> /docker-entrypoint.sh && \
    echo '' >> /docker-entrypoint.sh && \
    echo 'echo "=== Krown Agent Daemon ==="' >> /docker-entrypoint.sh && \
    echo 'echo "[Docker] Initialisation du conteneur..."' >> /docker-entrypoint.sh && \
    echo '' >> /docker-entrypoint.sh && \
    echo '# Créer le répertoire pour le socket' >> /docker-entrypoint.sh && \
    echo 'mkdir -p /run/krown' >> /docker-entrypoint.sh && \
    echo 'chmod 755 /run/krown' >> /docker-entrypoint.sh && \
    echo '' >> /docker-entrypoint.sh && \
    echo '# Démarrer systemd si disponible (mode privilégié)' >> /docker-entrypoint.sh && \
    echo 'if [ -d /run/systemd/system ] && [ "$(id -u)" = "0" ]; then' >> /docker-entrypoint.sh && \
    echo '    echo "[Docker] Démarrage de systemd..."' >> /docker-entrypoint.sh && \
    echo '    # Initialiser systemd' >> /docker-entrypoint.sh && \
    echo '    systemctl daemon-reload' >> /docker-entrypoint.sh && \
    echo '    systemctl enable krown-agent.service' >> /docker-entrypoint.sh && \
    echo '    systemctl start krown-agent.service' >> /docker-entrypoint.sh && \
    echo '    # Garder le conteneur en vie' >> /docker-entrypoint.sh && \
    echo '    exec /lib/systemd/systemd --system --unit=basic.target' >> /docker-entrypoint.sh && \
    echo 'else' >> /docker-entrypoint.sh && \
    echo '    echo "[Docker] Démarrage direct de l agent..."' >> /docker-entrypoint.sh && \
    echo '    echo "[Agent] Socket: ${SOCKET_PATH:-/run/krown/krown-agent.sock}"' >> /docker-entrypoint.sh && \
    echo '    exec /usr/local/bin/krown-agent "${SOCKET_PATH:-/run/krown/krown-agent.sock}"' >> /docker-entrypoint.sh && \
    echo 'fi' >> /docker-entrypoint.sh && \
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

