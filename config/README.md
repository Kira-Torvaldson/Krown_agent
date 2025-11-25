# Configuration Krown Agent

Ce répertoire contient les fichiers de configuration du projet.

## 📄 Fichiers

- **krown-agent.service** - Service systemd pour démarrage automatique
- **docker-compose.yml** - Configuration Docker Compose

## 🔧 Installation

### Service Systemd

```bash
sudo cp krown-agent.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable krown-agent.service
sudo systemctl start krown-agent.service
```

### Docker Compose

```bash
docker-compose -f config/docker-compose.yml up -d
```

Ou depuis la racine du projet :

```bash
docker-compose up -d
```

