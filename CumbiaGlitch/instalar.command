#!/bin/bash
# Instala CumbiaGlitch en el Mac. Doble clic (o clic derecho > Abrir la primera vez).
cd "$(dirname "$0")"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/CumbiaGlitch.vst3 ~/Library/Audio/Plug-Ins/Components/CumbiaGlitch.component
cp -R CumbiaGlitch.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R CumbiaGlitch.component ~/Library/Audio/Plug-Ins/Components/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/CumbiaGlitch.vst3
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/Components/CumbiaGlitch.component
killall -9 AudioComponentRegistrar 2>/dev/null
echo ""
echo "Listo. CumbiaGlitch instalado. Abre (o reinicia) Ableton Live."
echo "Puedes cerrar esta ventana."
