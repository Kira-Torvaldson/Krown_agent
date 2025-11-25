# Makefile pour Krown Agent (C + Rust)
# Compile le code C et la bibliothèque Rust, puis les lie ensemble

CC = gcc
CFLAGS = -Wall -Wextra -O3 -std=c11 -D_GNU_SOURCE -flto=auto -march=native -DNDEBUG -ffast-math -funroll-loops
LDFLAGS = -lssh -lpthread -lm -ljson-c -flto=auto -L./target/release -lkrown_memory -ldl -Wl,--gc-sections
RUST_SRC_DIR = src-rust
RUST_LIB = target/release/libkrown_memory.a
CARGO = cargo

# Répertoires
SRC_DIR = src
OBJ_DIR = build
BIN_DIR = bin

# Fichiers sources C
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
TARGET = $(BIN_DIR)/krown-agent

# Par défaut
.DEFAULT_GOAL := all
all: $(RUST_LIB) $(TARGET)
	@echo "✓ Build complet: $(TARGET)"

# Compilation Rust (optimisé)
$(RUST_LIB): $(RUST_SRC_DIR)/lib.rs Cargo.toml
	@echo "Building Rust library (optimized)..."
	@$(CARGO) build --release
	@echo "✓ Rust library built successfully"

# Compilation C
$(TARGET): $(OBJECTS) $(RUST_LIB) | $(BIN_DIR)
	@echo "Linking $@..."
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)
	@echo "✓ krown-agent built successfully"

# Compilation des fichiers objets C
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -I$(SRC_DIR) -I$(RUST_SRC_DIR) -c $< -o $@

# Créer les répertoires
$(OBJ_DIR):
	@mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

# Nettoyage
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(OBJ_DIR) $(BIN_DIR)
	@$(CARGO) clean 2>/dev/null || true
	@echo "✓ Cleaned build artifacts"

# Installation
install: $(TARGET)
	@echo "Installing krown-agent..."
	@cp $(TARGET) /usr/local/bin/
	@chmod +x /usr/local/bin/krown-agent
	@echo "✓ krown-agent installed to /usr/local/bin/"

# Installation du service systemd
install-service: config/krown-agent.service
	@echo "Installing systemd service..."
	@sudo cp config/krown-agent.service /etc/systemd/system/
	@sudo systemctl daemon-reload
	@echo "✓ Service installed. Use 'sudo systemctl enable krown-agent.service' to enable"

# Dépendances (Ubuntu/Debian)
deps:
	@echo "Installing dependencies..."
	@sudo apt-get update
	@sudo apt-get install -y libssh-dev libjson-c-dev build-essential
	@echo "Installing Rust..."
	@if ! command -v cargo &> /dev/null; then \
		curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y; \
	fi
	@echo "✓ Dependencies installed"

# Vérification
check: $(TARGET)
	@echo "Checking installation..."
	@test -f $(TARGET) && echo "✓ Binary exists" || echo "✗ Binary missing"
	@test -f $(RUST_LIB) && echo "✓ Rust library exists" || echo "✗ Rust library missing"
	@ldd $(TARGET) 2>/dev/null | grep -q libssh && echo "✓ libssh linked" || echo "✗ libssh not linked"

# Aide
help:
	@echo "Krown Agent - Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all              - Compile tout (défaut)"
	@echo "  clean            - Nettoie les fichiers de build"
	@echo "  install          - Installe le binaire dans /usr/local/bin"
	@echo "  install-service  - Installe le service systemd"
	@echo "  deps             - Installe les dépendances"
	@echo "  check            - Vérifie l'installation"
	@echo "  help             - Affiche cette aide"

.PHONY: all clean install install-service deps check help
