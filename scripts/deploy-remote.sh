#!/bin/bash
# Déploiement automatique sur machine distante via SSH
# Usage: ./scripts/deploy-remote.sh user@host [socket_path]

set -e

if [ $# -lt 1 ]; then
    echo "Usage: $0 user@host [socket_path]"
    exit 1
fi

REMOTE_HOST="$1"
SOCKET_PATH="${2:-/run/krown/krown-agent.sock}"
# Obtenir le répertoire parent (agent/) depuis scripts/
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "=== Déploiement automatique de Krown Agent ==="
echo "Machine cible: $REMOTE_HOST"
echo "Socket: $SOCKET_PATH"
echo ""

# Créer une archive du projet
echo "📦 Préparation de l'archive..."
TMP_DIR=$(mktemp -d)
ARCHIVE="$TMP_DIR/krown-agent.tar.gz"

tar -czf "$ARCHIVE" \
    --exclude='.git' \
    --exclude='bin' \
    --exclude='build' \
    --exclude='target' \
    --exclude='*.o' \
    --exclude='*.a' \
    -C "$SCRIPT_DIR" .

echo "✅ Archive créée: $(basename $ARCHIVE)"

# Copier l'archive sur la machine distante
echo ""
echo "📤 Envoi vers $REMOTE_HOST..."
scp "$ARCHIVE" "$REMOTE_HOST:/tmp/krown-agent.tar.gz"

# Exécuter l'installation sur la machine distante
echo ""
echo "🔧 Installation sur la machine distante..."
ssh "$REMOTE_HOST" << EOF
set -e
cd /tmp
rm -rf krown-agent
mkdir -p krown-agent
tar -xzf krown-agent.tar.gz -C krown-agent
cd krown-agent

# Vérifier Docker
if ! command -v docker &> /dev/null; then
    echo "Installation de Docker..."
    if [ -f /etc/debian_version ]; then
        sudo apt-get update
        sudo apt-get install -y docker.io docker-compose
        sudo systemctl enable docker
        sudo systemctl start docker
    elif [ -f /etc/redhat-release ]; then
        sudo yum install -y docker docker-compose
        sudo systemctl enable docker
        sudo systemctl start docker
    fi
fi

# Créer le répertoire pour le socket
SOCKET_DIR=\$(dirname "$SOCKET_PATH")
sudo mkdir -p "\$SOCKET_DIR"
sudo chmod 755 "\$SOCKET_DIR"

# Construire et démarrer
echo "Construction de l'image..."
docker build -t krown-agent .

# Arrêter le conteneur existant si présent
if docker ps -a --format '{{.Names}}' | grep -q "^krown-agent$"; then
    docker stop krown-agent 2>/dev/null || true
    docker rm krown-agent 2>/dev/null || true
fi

echo "Démarrage du conteneur..."
docker run -d \\
    --name krown-agent \\
    --restart=always \\
    -v "\$SOCKET_DIR:\$SOCKET_DIR" \\
    -v /tmp:/tmp \\
    -e SOCKET_PATH="$SOCKET_PATH" \\
    -e RUST_LOG=info \\
    krown-agent

echo "✅ Installation terminée sur $REMOTE_HOST"
echo "Socket: $SOCKET_PATH"
EOF

# Nettoyer
rm -rf "$TMP_DIR"

echo ""
echo "✅ Déploiement terminé!"
echo ""
echo "Vérification:"
ssh "$REMOTE_HOST" "docker ps --filter name=krown-agent && test -S $SOCKET_PATH && echo '✅ Socket actif' || echo '⚠️  Socket non trouvé'"

