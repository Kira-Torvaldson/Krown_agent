# Script d'installation automatique de Krown Agent via Docker (Windows)
# Usage: .\install.ps1 [socket_path]

param(
    [string]$SocketPath = "C:\tmp\krown-agent.sock"
)

$ErrorActionPreference = "Stop"

Write-Host "=== Installation automatique de Krown Agent ===" -ForegroundColor Cyan
Write-Host ""

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ImageName = "krown-agent"
$ContainerName = "krown-agent"

# Vérifier que Docker est installé
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    Write-Host "❌ Docker n'est pas installé" -ForegroundColor Red
    Write-Host "Installez Docker Desktop depuis: https://www.docker.com/products/docker-desktop" -ForegroundColor Yellow
    exit 1
}

Write-Host "✅ Docker installé" -ForegroundColor Green

# Vérifier que Docker fonctionne
try {
    docker info | Out-Null
    Write-Host "✅ Docker fonctionne" -ForegroundColor Green
} catch {
    Write-Host "❌ Docker n'est pas en cours d'exécution" -ForegroundColor Red
    Write-Host "Démarrez Docker Desktop" -ForegroundColor Yellow
    exit 1
}

# Créer le répertoire pour le socket
$SocketDir = Split-Path -Parent $SocketPath
Write-Host "Création du répertoire: $SocketDir" -ForegroundColor Cyan
New-Item -ItemType Directory -Force -Path $SocketDir | Out-Null

# Construire l'image Docker
Write-Host ""
Write-Host "🔨 Construction de l'image Docker..." -ForegroundColor Cyan
Set-Location $ScriptDir
docker build -t $ImageName .

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Échec de la construction de l'image" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Image Docker construite" -ForegroundColor Green

# Arrêter et supprimer le conteneur existant s'il existe
if (docker ps -a --format '{{.Names}}' | Select-String -Pattern "^${ContainerName}$") {
    Write-Host "Arrêt du conteneur existant..." -ForegroundColor Yellow
    docker stop $ContainerName 2>$null
    docker rm $ContainerName 2>$null
}

# Démarrer le conteneur
Write-Host ""
Write-Host "🚀 Démarrage du conteneur..." -ForegroundColor Cyan
docker run -d `
    --name $ContainerName `
    --restart=always `
    --privileged `
    -v "${SocketDir}:${SocketDir}" `
    -v "C:\tmp:C:\tmp" `
    -e "SOCKET_PATH=$SocketPath" `
    -e "RUST_LOG=info" `
    $ImageName

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Échec du démarrage du conteneur" -ForegroundColor Red
    exit 1
}

Write-Host "✅ Conteneur démarré" -ForegroundColor Green

# Vérifier le statut
Write-Host ""
Write-Host "📊 Statut du conteneur:" -ForegroundColor Cyan
docker ps --filter "name=$ContainerName" --format "table {{.Names}}\t{{.Status}}\t{{.Ports}}"

Write-Host ""
Write-Host "✅ Installation terminée!" -ForegroundColor Green
Write-Host ""
Write-Host "Commandes utiles:" -ForegroundColor Yellow
Write-Host "  - Voir les logs: docker logs -f $ContainerName"
Write-Host "  - Arrêter: docker stop $ContainerName"
Write-Host "  - Redémarrer: docker restart $ContainerName"
Write-Host "  - Statut: docker ps --filter name=$ContainerName"
Write-Host ""
Write-Host "Le conteneur redémarrera automatiquement au boot de la machine." -ForegroundColor Cyan

