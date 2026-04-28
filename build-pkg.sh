#!/bin/bash
# Script para compilar Apollo e criar pacote para Arch/CachyOS

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/cmake-build-debug"  # Usar o build existente
PKG_DIR="${SCRIPT_DIR}/pkg-output"

echo "=== Criando pacote Apollo para Arch/CachyOS ==="

# Verificar se o build existe
if [ ! -f "${BUILD_DIR}/sunshine" ]; then
    echo "Erro: Build não encontrado em ${BUILD_DIR}"
    echo "Execute primeiro: cmake -B cmake-build-debug && cmake --build cmake-build-debug"
    exit 1
fi

# Limpar diretório de pacote anterior
rm -rf "${PKG_DIR}"
mkdir -p "${PKG_DIR}"

# Instalar em diretório temporário
echo "=== Instalando arquivos ==="
DESTDIR="${PKG_DIR}" ninja -C "${BUILD_DIR}" install

# Renomear executável (remover symlink e renomear o real)
if [ -L "${PKG_DIR}/usr/bin/sunshine" ]; then
    # sunshine é um symlink, precisamos do arquivo real
    REAL_EXE=$(readlink -f "${PKG_DIR}/usr/bin/sunshine")
    rm "${PKG_DIR}/usr/bin/sunshine"
    mv "${REAL_EXE}" "${PKG_DIR}/usr/bin/apollo"
elif [ -f "${PKG_DIR}/usr/bin/sunshine" ]; then
    mv "${PKG_DIR}/usr/bin/sunshine" "${PKG_DIR}/usr/bin/apollo"
fi

# Remover qualquer executável sunshine restante
rm -f "${PKG_DIR}/usr/bin/sunshine-"* 2>/dev/null || true

# Instalar ícones manualmente para garantir que estejam presentes
echo "=== Instalando ícones ==="
ICON_DIR="${PKG_DIR}/usr/share/icons/hicolor/scalable"
mkdir -p "${ICON_DIR}/apps"
mkdir -p "${ICON_DIR}/status"

# Ícone principal do app
cp "${SCRIPT_DIR}/apollo.svg" "${ICON_DIR}/apps/apollo.svg"

# Ícones do tray
cp "${SCRIPT_DIR}/apollo.svg" "${ICON_DIR}/status/apollo-tray.svg"
cp "${SCRIPT_DIR}/src_assets/common/assets/web/public/images/apollo-playing.svg" "${ICON_DIR}/status/"
cp "${SCRIPT_DIR}/src_assets/common/assets/web/public/images/apollo-pausing.svg" "${ICON_DIR}/status/"
cp "${SCRIPT_DIR}/src_assets/common/assets/web/public/images/apollo-locked.svg" "${ICON_DIR}/status/"

echo "Ícones instalados:"
ls -la "${ICON_DIR}/apps/"
ls -la "${ICON_DIR}/status/"

# Criar pacote .pkg.tar.zst manualmente
echo "=== Criando pacote ==="

VERSION="0.1.0"
PKG_NAME="apollo-${VERSION}-1-x86_64.pkg.tar.zst"

cd "${PKG_DIR}"

# Criar .PKGINFO
cat > .PKGINFO << EOF
pkgname = apollo
pkgver = ${VERSION}-1
pkgdesc = Self-hosted game streaming server with virtual display support
url = https://github.com/ClassicOldSong/Apollo
builddate = $(date +%s)
packager = Local Build
size = $(du -sb usr | cut -f1)
arch = x86_64
license = GPL-3.0-only
install = .INSTALL
depend = avahi
depend = curl
depend = evdi
depend = libayatana-appindicator
depend = libcap
depend = libdrm
depend = libevdev
depend = libnotify
depend = libpulse
depend = libva
depend = libx11
depend = libxcb
depend = libxfixes
depend = libxrandr
depend = libxtst
depend = miniupnpc
depend = numactl
depend = openssl
depend = opus
depend = udev
optdepend = cuda: NVIDIA GPU encoding support
optdepend = libva-mesa-driver: AMD GPU encoding support
provides = sunshine
conflict = sunshine
EOF

# Criar .INSTALL (script pós-instalação)
cat > .INSTALL << 'EOF'
do_setcap() {
  setcap cap_sys_admin+p $(readlink -f usr/bin/apollo)
}

do_udev_reload() {
  udevadm control --reload-rules
  udevadm trigger --property-match=DEVNAME=/dev/uinput
  udevadm trigger --property-match=DEVNAME=/dev/uhid
  modprobe uinput || true
  modprobe uhid || true
}

load_evdi() {
  # Carregar módulo EVDI para virtual display
  modprobe evdi || true
  
  # Adicionar ao carregamento automático no boot
  if [ ! -f /etc/modules-load.d/evdi.conf ]; then
    echo "evdi" > /etc/modules-load.d/evdi.conf
  fi
}

post_install() {
  do_setcap
  do_udev_reload
  load_evdi
  echo ""
  echo "==> Apollo instalado com sucesso!"
  echo "==> Virtual display (EVDI) configurado automaticamente."
  echo "==> Execute 'apollo' para iniciar o servidor."
  echo ""
}

post_upgrade() {
  do_setcap
  do_udev_reload
  load_evdi
}
EOF

# Criar pacote
tar --zstd -cf "${SCRIPT_DIR}/${PKG_NAME}" .PKGINFO .INSTALL usr

echo ""
echo "=== Pacote criado com sucesso! ==="
echo "Arquivo: ${SCRIPT_DIR}/${PKG_NAME}"
echo ""
echo "Para instalar:"
echo "  sudo pacman -U ${PKG_NAME}"
echo ""
echo "O pacote irá automaticamente:"
echo "  - Instalar EVDI como dependência"
echo "  - Configurar capabilities para KMS capture"
echo "  - Carregar módulo EVDI"
echo "  - Configurar EVDI para carregar no boot"
echo ""
