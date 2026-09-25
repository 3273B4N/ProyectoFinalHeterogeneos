# ==============================================================================
# Makefile for NEAT & SnakeNEAT - Ubuntu/Debian and Arch Linux
# The distribution is detected automatically (pacman -> Arch, otherwise apt).
# SFML and the font are located automatically wherever they live on this
# machine (pkg-config first, then a list of common install prefixes).
#
#   make         -> deps + font + text sizes + build NEAT + Snake
#   make run     -> same, then launch SnakeNEAT
#   make deps    -> (re)install system dependencies
#   make clean   -> remove build artifacts
#   make info    -> show detected distro, tools and settings
#
# Requires: sudo, internet access, and a graphical session for `make run`.
# Force a distro if detection is wrong:  make DISTRO=arch  /  make DISTRO=debian
# ==============================================================================

ROOT_DIR    := $(CURDIR)
BUILD_DIR   := $(ROOT_DIR)/build
SNAKE_DIR   := $(ROOT_DIR)/examples/snake
SNAKE_BUILD := $(SNAKE_DIR)/build
GENOME_SRC  := $(ROOT_DIR)/src/genome.cpp

DISTRO ?= $(if $(shell command -v pacman 2>/dev/null),arch,debian)

# Use the system cmake explicitly: avoids a broken copy in /usr/local/bin or ~/.local/bin
CMAKE ?= /usr/bin/cmake
JOBS  ?= $(shell nproc 2>/dev/null || echo 2)
SUDO  := $(if $(filter 0,$(shell id -u)),,sudo)

CMAKE_FLAGS := -Wno-dev -Wno-deprecated --no-warn-unused-cli \
               -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
               -DNEAT_WERROR=OFF

# ------------------------------------------------------------------------------
# SFML 2 discovery (any machine): pkg-config first, then common prefixes.
# ------------------------------------------------------------------------------
PKGCONFIG ?= pkg-config
SFML_PC_OK := $(shell $(PKGCONFIG) --exists sfml-graphics sfml-window sfml-system sfml-audio 2>/dev/null && echo yes)

ifeq ($(SFML_PC_OK),yes)
    SFML_VERSION := $(shell $(PKGCONFIG) --modversion sfml-graphics 2>/dev/null)
else
    # No usable .pc file: search a handful of common install locations.
    # Add more candidates here if your distro puts SFML somewhere else.
    SFML_CANDIDATES := /opt/sfml2 /usr/local /usr
    SFML_PREFIX := $(firstword $(foreach p,$(SFML_CANDIDATES),\
                      $(if $(wildcard $(p)/include/SFML/Graphics.hpp),$(p),)))
endif

ifdef SFML_PREFIX
    export CMAKE_PREFIX_PATH := $(SFML_PREFIX):$(CMAKE_PREFIX_PATH)
    export CXXFLAGS := -I$(SFML_PREFIX)/include $(CXXFLAGS)
    export LDFLAGS  := -L$(SFML_PREFIX)/lib -Wl,-rpath,$(SFML_PREFIX)/lib $(LDFLAGS)
    CMAKE_FLAGS += -DCMAKE_INCLUDE_PATH="$(SFML_PREFIX)/include" \
                   -DCMAKE_LIBRARY_PATH="$(SFML_PREFIX)/lib" \
                   -DCMAKE_INSTALL_RPATH="$(SFML_PREFIX)/lib" \
                   -DCMAKE_BUILD_WITH_INSTALL_RPATH=TRUE
    RUN_ENV := LD_LIBRARY_PATH="$(SFML_PREFIX)/lib:$$LD_LIBRARY_PATH"
else
    RUN_ENV :=
endif

# ------------------------------------------------------------------------------
# Distro-specific package lists (only used for `make deps`)
# ------------------------------------------------------------------------------
ifeq ($(DISTRO),arch)
    ARCH_PKGS  := base-devel cmake pkgconf git
    CHECK_DEPS_CMD := pacman -Q $(ARCH_PKGS)
else
    APT_PACKAGES := build-essential cmake pkg-config git libsfml-dev
    CHECK_DEPS_CMD := dpkg -s $(APT_PACKAGES)
endif

# ------------------------------------------------------------------------------
# Font discovery (any machine): ask fontconfig for a real sans-serif font
# instead of assuming a fixed path. Falls back to a short candidate list.
# ------------------------------------------------------------------------------
FC_FONT := $(shell fc-match -f '%{file}' sans-serif 2>/dev/null)

FONT_CANDIDATES := \
    /usr/share/fonts/truetype/dejavu/DejaVuSans.ttf \
    /usr/share/fonts/dejavu/DejaVuSans.ttf \
    /usr/share/fonts/TTF/DejaVuSans.ttf \
    /usr/share/fonts/cantarell/Cantarell-VF.otf \
    /usr/share/fonts/noto/NotoSans-Regular.ttf

ifneq ($(FC_FONT),)
    FONT_PATH := $(FC_FONT)
else
    FONT_PATH := $(firstword $(foreach f,$(FONT_CANDIDATES),$(wildcard $(f))))
endif

# Rough size heuristic: DejaVu/Noto glyphs render bigger than Cantarell at the
# same point size, so shrink a bit when we're not sure it's Cantarell.
ifneq ($(findstring antarell,$(FONT_PATH)),)
    override NODE_FONT_SIZE ?= 20
    override LIST_FONT_SIZE ?= 15
else
    override NODE_FONT_SIZE ?= 12
    override LIST_FONT_SIZE ?= 12
endif

.PHONY: all check-deps deps deps-arch deps-debian font textsize \
        neat snake run clean info help

all: neat snake ## Build NEAT + SnakeNEAT (installs deps/font if missing)

# Installs dependencies only if some package is missing (works on any machine)
check-deps:
	@$(CHECK_DEPS_CMD) >/dev/null 2>&1 || $(MAKE) --no-print-directory deps

deps: deps-$(DISTRO) ## Install system dependencies (apt or pacman+AUR)
	@$(CMAKE) --version >/dev/null 2>&1 || { \
		echo "ERROR: $(CMAKE) does not work."; \
		echo "Reinstall cmake with your package manager."; \
		exit 1; }
	@echo "--> [SUCCESS] Dependencies ready ($$($(CMAKE) --version | head -n1))"

deps-debian:
	@echo "--> Installing (apt): $(APT_PACKAGES)"
	$(SUDO) apt-get update
	$(SUDO) apt-get install -y $(APT_PACKAGES)

deps-arch:
	@echo "--> Installing (pacman): $(ARCH_PKGS)"
	$(SUDO) pacman -S --needed --noconfirm $(ARCH_PKGS)
	@if ! $(PKGCONFIG) --exists sfml-graphics 2>/dev/null && [ ! -d /opt/sfml2 ] && [ ! -d /usr/local/include/SFML ]; then \
		echo "--> SFML 2 not found: installing from the AUR (run as a normal user, not root)"; \
		if command -v yay >/dev/null 2>&1; then yay -S --needed --noconfirm sfml2; \
		elif command -v paru >/dev/null 2>&1; then paru -S --needed --noconfirm sfml2; \
		else \
			echo "--> No AUR helper found: building sfml2 with makepkg"; \
			rm -rf /tmp/sfml2-aur && \
			git clone https://aur.archlinux.org/sfml2.git /tmp/sfml2-aur && \
			cd /tmp/sfml2-aur && makepkg -si --noconfirm; \
		fi; \
	fi

font: ## Report which font drawNetwork will use (no system changes needed)
	@if [ -z "$(FONT_PATH)" ]; then \
		echo "ERROR: no usable font found (fc-match failed and no candidate exists)."; \
		echo "Install a font package, e.g.: sudo apt install fonts-dejavu-core"; \
		exit 1; \
	else \
		echo "--> Using font: $(FONT_PATH)"; \
	fi

textsize: font ## Point drawNetwork() at the detected font and apply sizes
	@sed -i -e "s#\(loadFromFile(\"\)[^\"]*\(\.\(ttf\|otf\)\"\)#\1$(FONT_PATH)\2#" $(GENOME_SRC) 2>/dev/null || true
	@grep -q 'dotsText\[i\]\.setCharacterSize($(NODE_FONT_SIZE))' $(GENOME_SRC) && \
	grep -q 'mainText\.setCharacterSize($(LIST_FONT_SIZE))' $(GENOME_SRC) || { \
		echo "--> Setting text sizes: nodes=$(NODE_FONT_SIZE), list=$(LIST_FONT_SIZE)"; \
		sed -i -e 's/\(dotsText\[i\]\.setCharacterSize(\)[0-9]*)/\1$(NODE_FONT_SIZE))/' \
		      -e 's/\(mainText\.setCharacterSize(\)[0-9]*)/\1$(LIST_FONT_SIZE))/' $(GENOME_SRC); }

neat: check-deps textsize ## Compile libneat.a (also copied to lib/)
	@echo "--> Building NEAT static library..."
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && $(CMAKE) .. $(CMAKE_FLAGS)
	@$(MAKE) -C $(BUILD_DIR) -j$(JOBS)
	@cp -f $(BUILD_DIR)/libneat.a $(ROOT_DIR)/libneat.a
	@mkdir -p $(ROOT_DIR)/lib
	@cp -f $(BUILD_DIR)/libneat.a $(ROOT_DIR)/lib/libneat.a
	@echo "--> [SUCCESS] Generated libneat.a"

snake: neat ## Compile SnakeNEAT example
	@echo "--> Building SnakeNEAT example..."
	@mkdir -p $(SNAKE_BUILD)
	@cd $(SNAKE_BUILD) && $(CMAKE) .. $(CMAKE_FLAGS)
	@$(MAKE) -C $(SNAKE_BUILD) -j$(JOBS)
	@echo "--> [SUCCESS] Generated SnakeNEAT executable"

run: snake ## Launch the SnakeNEAT simulation
	@echo "--> Launching SnakeNEAT simulation..."
	@$(RUN_ENV) $(SNAKE_BUILD)/SnakeNEAT

clean: ## Remove build directories and libneat.a (do this before sharing)
	@echo "--> Removing build artifacts..."
	@rm -rf $(BUILD_DIR) $(SNAKE_BUILD) $(ROOT_DIR)/libneat.a
	@echo "--> [CLEAN COMPLETE]"

info: ## Show detected distro, tools and settings
	@echo "=================================================="
	@echo "       NEAT Project Build Configuration"
	@echo "=================================================="
	@echo "Distro       : $(DISTRO)"
	@echo "CMake binary : $(CMAKE)"
	@echo "CMake version: $$($(CMAKE) --version 2>&1 | head -n1)"
	@echo "Compiler     : $$(g++ --version 2>&1 | head -n1)"
	@echo "SFML source  : $(if $(filter yes,$(SFML_PC_OK)),pkg-config ($(SFML_VERSION)),$(if $(SFML_PREFIX),$(SFML_PREFIX),NOT FOUND))"
	@echo "Font in use  : $(if $(FONT_PATH),$(FONT_PATH),NOT FOUND)"
	@echo "Text sizes   : nodes=$(NODE_FONT_SIZE) list=$(LIST_FONT_SIZE)"
	@echo "Jobs         : $(JOBS)"
	@echo "=================================================="

help: ## Display available targets
	@echo "Usage: make [target]"
	@echo ""
	@echo "Targets:"
	@grep -E '^[a-zA-Z_-]+:.*?## .*$$' $(MAKEFILE_LIST) | awk 'BEGIN {FS = ":.*?## "}; {printf "  \033[36m%-10s\033[0m %s\n", $$1, $$2}'
