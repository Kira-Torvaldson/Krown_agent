#!/bin/bash
# Script d'installation automatique de Krown Agent via Docker
# Usage: ./scripts/install.sh [socket_path]

set -e

SOCKET_PATH="${1:-/run/krown/krown-agent.sock}"
# Obtenir le répertoire parent (agent/) depuis scripts/
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
IMAGE_NAME="krown-agent"
CONTAINER_NAME="krown-agent"

echo "=== Installation automatique de Krown Agent ==="
echo ""

# Vérifier que Docker est installé
if ! command -v docker &> /dev/null; then
    echo "❌ Docker n'est pas installé"
    echo "Installation de Docker..."
    
    # Détecter la distribution
    if [ -f /etc/debian_version ]; then
        echo "Distribution Debian/Ubuntu détectée"
        sudo apt-get update
        sudo apt-get install -y docker.io docker-compose
        sudo systemctl enable docker
        sudo systemctl start docker
    elif [ -f /etc/redhat-release ]; then
        echo "Distribution RedHat/CentOS détectée"
        sudo yum install -y docker docker-compose
        sudo systemctl enable docker
        sudo systemctl start docker
    else
        echo "❌ Distribution non supportée. Installez Docker manuellement."
        exit 1
    fi
fi

echo "✅ Docker installé"

# Vérifier que Docker fonctionne
if ! docker info &> /dev/null; then
    echo "❌ Docker n'est pas en cours d'exécution"
    echo "Démarrage de Docker..."
    sudo systemctl start docker
fi

echo "✅ Docker fonctionne"

# Créer le répertoire pour le socket
SOCKET_DIR=$(dirname "$SOCKET_PATH")
echo "Création du répertoire: $SOCKET_DIR"
sudo mkdir -p "$SOCKET_DIR"
sudo chmod 755 "$SOCKET_DIR"

# Construire l'image Docker
echo ""
echo "🔨 Construction de l'image Docker..."
cd "$SCRIPT_DIR"
docker build -t "$IMAGE_NAME" .

if [ $? -ne 0 ]; then
    echo "❌ Échec de la construction de l'image"
    exit 1
fi

echo "✅ Image Docker construite"

# Arrêter et supprimer le conteneur existant s'il existe
if docker ps -a --format '{{.Names}}' | grep -q "^${CONTAINER_NAME}$"; then
    echo "Arrêt du conteneur existant..."
    docker stop "$CONTAINER_NAME" 2>/dev/null || true
    docker rm "$CONTAINER_NAME" 2>/dev/null || true
fi

# Démarrer le conteneur
echo ""
echo "🚀 Démarrage du conteneur..."
docker run -d \
    --name "$CONTAINER_NAME" \
    --restart=always \
    --privileged \
    -v "$SOCKET_DIR:$SOCKET_DIR" \
    -v /tmp:/tmp \
    -e SOCKET_PATH="$SOCKET_PATH" \
    -e RUST_LOG=info \
    "$IMAGE_NAME"

if [ $? -ne 0 ]; then
    echo "❌ Échec du démarrage du conteneur"
    exit 1
fi

echo "✅ Conteneur démarré"

# Attendre que le socket soit créé
echo ""
echo "⏳ Attente de la création du socket..."
for i in {1..10}; do
    if [ -S "$SOCKET_PATH" ]; then
        echo "✅ Socket créé: $SOCKET_PATH"
        break
    fi
    sleep 1
done

if [ ! -S "$SOCKET_PATH" ]; then
    echo "⚠️  Le socket n'a pas été créé après 10 secondes"
    echo "Vérifiez les logs: docker logs $CONTAINER_NAME"
fi

# Vérifier le statut
echo ""
echo "📊 Statut du conteneur:"
docker ps --filter "name=$CONTAINER_NAME" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"

echo ""
echo "✅ Installation terminée!"
echo ""
echo "Commandes utiles:"
echo "  - Voir les logs: docker logs -f $CONTAINER_NAME"
echo "  - Arrêter: docker stop $CONTAINER_NAME"
echo "  - Redémarrer: docker restart $CONTAINER_NAME"
echo "  - Statut: docker ps --filter name=$CONTAINER_NAME"
echo ""
echo "Le conteneur redémarrera automatiquement au boot de la machine."

