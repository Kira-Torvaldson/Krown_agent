#!/bin/bash
# Installation ultra-rapide en une commande
# Usage: curl -sSL https://raw.githubusercontent.com/Kira-Torvaldson/Krown_agent/main/scripts/quick-install.sh | bash

set -e

echo "🚀 Installation rapide de Krown Agent..."

# Obtenir le répertoire du projet (agent/)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Vérifier Docker
if ! command -v docker &> /dev/null; then
    echo "❌ Docker requis. Installez-le d'abord: https://docs.docker.com/get-docker/"
    exit 1
fi

# Cloner ou utiliser le répertoire actuel
if [ ! -f "Dockerfile" ]; then
    if [ -d ".git" ]; then
        echo "📦 Utilisation du dépôt local"
    else
        echo "📦 Clonage du dépôt..."
        git clone https://github.com/Kira-Torvaldson/Krown_agent.git /tmp/krown-agent
        cd /tmp/krown-agent/agent
    fi
fi

# Construire et démarrer
echo "🔨 Construction de l'image..."
docker build -t krown-agent .

echo "🚀 Démarrage du conteneur..."
docker run -d \
    --name krown-agent \
    --restart=always \
    -v /run/krown:/run/krown \
    -v /tmp:/tmp \
    -e SOCKET_PATH=/run/krown/krown-agent.sock \
    krown-agent

echo "✅ Installation terminée!"
echo "Socket: /run/krown/krown-agent.sock"
echo "Logs: docker logs -f krown-agent"

