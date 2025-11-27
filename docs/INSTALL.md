# Installation Automatique de Krown Agent

## 🚀 Installation Rapide

### Linux

```bash
# Rendre le script exécutable
chmod +x scripts/install.sh

# Installer automatiquement
./scripts/install.sh

# Ou avec un chemin de socket personnalisé
./scripts/install.sh /custom/path/krown-agent.sock
```

### Windows (PowerShell)

```powershell
# Exécuter le script
.\scripts\install.ps1

# Ou avec un chemin de socket personnalisé
.\scripts\install.ps1 -SocketPath "C:\tmp\krown-agent.sock"
```

## 📋 Ce que fait le script

1. ✅ **Vérifie Docker** : Installe Docker si nécessaire
2. ✅ **Construit l'image** : Compile l'agent dans une image Docker
3. ✅ **Crée les répertoires** : Prépare l'environnement
4. ✅ **Démarre le conteneur** : Lance l'agent avec redémarrage automatique
5. ✅ **Vérifie le statut** : Confirme que tout fonctionne

## 🔄 Redémarrage Automatique

Le conteneur est configuré avec `--restart=always`, ce qui signifie :
- ✅ Redémarre automatiquement au boot de la machine
- ✅ Redémarre en cas de crash
- ✅ Redémarre après un redémarrage de Docker

## 📝 Configuration

### Variables d'Environnement

- `SOCKET_PATH` : Chemin du socket Unix (défaut: `/run/krown/krown-agent.sock`)
- `RUST_LOG` : Niveau de log Rust (défaut: `info`)

### Volumes

- Socket directory : Monté pour accès depuis l'hôte
- `/tmp` : Répertoire temporaire

## 🛠️ Dépannage

### Le conteneur ne démarre pas

```bash
# Voir les logs
docker logs krown-agent

# Vérifier les erreurs
docker logs krown-agent 2>&1 | tail -50
```

### Le socket n'est pas accessible

```bash
# Vérifier que le socket existe
ls -la /run/krown/krown-agent.sock

# Vérifier les permissions
docker exec krown-agent ls -la /run/krown/
```

### Réinstaller

```bash
# Arrêter et supprimer
docker stop krown-agent
docker rm krown-agent

# Relancer le script d'installation
./scripts/install.sh
```

## 📦 Installation Manuelle

Si vous préférez installer manuellement :

```bash
# Construire l'image
docker build -t krown-agent .

# Démarrer le conteneur
docker run -d \
  --name krown-agent \
  --restart=always \
  --privileged \
  -v /run/krown:/run/krown \
  -v /tmp:/tmp \
  -e SOCKET_PATH=/run/krown/krown-agent.sock \
  krown-agent
```

## ✅ Vérification

```bash
# Vérifier que le conteneur tourne
docker ps | grep krown-agent

# Vérifier les logs
docker logs krown-agent

# Tester le socket
test -S /run/krown/krown-agent.sock && echo "✅ Socket actif"
```

