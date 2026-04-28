# Maintainer: SudoMaker
# Apollo - Game streaming server with virtual display support

pkgname=apollo
pkgver=0.1.0
pkgrel=1
pkgdesc="Self-hosted game streaming server with virtual display support"
arch=('x86_64')
url='https://github.com/ClassicOldSong/Apollo'
license=('GPL-3.0-only')
install=apollo.install

depends=(
  'avahi'
  'curl'
  'evdi'
  'libayatana-appindicator'
  'libcap'
  'libdrm'
  'libevdev'
  'libnotify'
  'libpulse'
  'libva'
  'libx11'
  'libxcb'
  'libxfixes'
  'libxrandr'
  'libxtst'
  'miniupnpc'
  'numactl'
  'openssl'
  'opus'
  'udev'
)

optdepends=(
  'cuda: NVIDIA GPU encoding support'
  'libva-mesa-driver: AMD GPU encoding support'
)

provides=('sunshine')
conflicts=('sunshine')

# Usar o build local já compilado
source=()
sha256sums=()

package() {
    # Copiar do build debug
    cd "${startdir}"
    
    # Executável
    install -Dm755 "cmake-build-debug/sunshine" "${pkgdir}/usr/bin/apollo"
    
    # Assets
    install -dm755 "${pkgdir}/usr/share/apollo"
    cp -r cmake-build-debug/assets/* "${pkgdir}/usr/share/apollo/"
    
    # Ícones
    install -Dm644 "apollo.svg" "${pkgdir}/usr/share/icons/hicolor/scalable/apps/apollo.svg"
    install -Dm644 "apollo.svg" "${pkgdir}/usr/share/icons/hicolor/scalable/status/apollo-tray.svg"
    install -Dm644 "src_assets/common/assets/web/public/images/apollo-playing.svg" \
        "${pkgdir}/usr/share/icons/hicolor/scalable/status/apollo-playing.svg"
    install -Dm644 "src_assets/common/assets/web/public/images/apollo-pausing.svg" \
        "${pkgdir}/usr/share/icons/hicolor/scalable/status/apollo-pausing.svg"
    install -Dm644 "src_assets/common/assets/web/public/images/apollo-locked.svg" \
        "${pkgdir}/usr/share/icons/hicolor/scalable/status/apollo-locked.svg"
    
    # Udev rules
    install -Dm644 "src_assets/linux/misc/60-sunshine.rules" \
        "${pkgdir}/usr/lib/udev/rules.d/60-apollo.rules"
    
    # Systemd service
    install -Dm644 "cmake-build-debug/sunshine.service" \
        "${pkgdir}/usr/lib/systemd/user/apollo.service"
    
    # Desktop file
    install -Dm644 "cmake-build-debug/dev.lizardbyte.app.Sunshine.desktop" \
        "${pkgdir}/usr/share/applications/apollo.desktop"
    
    # Modificar desktop file
    sed -i 's/sunshine/apollo/g; s/Sunshine/Apollo/g' \
        "${pkgdir}/usr/share/applications/apollo.desktop"
}
