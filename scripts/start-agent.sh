#!/bin/bash
# Script de démarrage de l'agent Krown
# Utilisation: ./start-agent.sh [socket_path]

set -e

SOCKET_PATH="${1:-/run/krown/krown-agent.sock}"
AGENT_BIN="/usr/local/bin/krown-agent"

echo "=== Krown Agent Startup Script ==="
echo "[Script] Démarrage de l'agent..."
echo "[Script] Socket: $SOCKET_PATH"

# Créer le répertoire pour le socket si nécessaire
SOCKET_DIR=$(dirname "$SOCKET_PATH")
if [ ! -d "$SOCKET_DIR" ]; then
    echo "[Script] Création du répertoire: $SOCKET_DIR"
    mkdir -p "$SOCKET_DIR"
    chmod 755 "$SOCKET_DIR"
fi

# Vérifier que le binaire existe
if [ ! -f "$AGENT_BIN" ]; then
    echo "[Script] ERREUR: Binaire introuvable: $AGENT_BIN"
    exit 1
fi

# Vérifier les permissions
if [ ! -x "$AGENT_BIN" ]; then
    echo "[Script] ERREUR: Binaire non exécutable: $AGENT_BIN"
    exit 1
fi

# Démarrer l'agent
echo "[Script] Lancement de l'agent..."
exec "$AGENT_BIN" "$SOCKET_PATH"

