# CumbiaGlitch

Sampler de dos canales para mezclar un sample de cumbia (canal A) con un beat de drum and bass o hip hop (canal B) y romperlo todo con un motor glitch de estilo IDM.

Formatos: **VST3** y **AU** para Mac (Apple Silicon e Intel). Funciona en Ableton Live, Logic, Reaper, etc.

## Controles

**Canal A (cumbia) y canal B (beat)**
- **Cargar sample**: elige un audio (WAV, AIFF, FLAC, MP3…) o arrástralo encima del panel.
- **Onda**: haz clic y arrastra los marcadores blancos para elegir el fragmento que suena.
- **Inicio / Fin**: lo mismo que los marcadores, como perillas (automatizables en Ableton).
- **Filtro lowpass**: corta los agudos del canal. Al máximo (20 kHz) suena limpio.
- **Compases + Sync al tempo**: con Sync activado, el fragmento elegido se estira o comprime para durar exactamente esos compases al tempo de tu proyecto. Así la cumbia y el drum and bass quedan cuadrados aunque tengan BPM distintos. Sin Sync, suena a su velocidad original.

**Centro**
- **Modo Loop**: suena en bucle cuando le das play en Ableton, siempre sincronizado al proyecto.
- **Modo MIDI**: suena mientras tocas una tecla. C3 (Do central) es el tono original; otras teclas lo suben o bajan.
- **Dry / Wet**: en Dry suenan los canales limpios; en Wet se aplica todo el efecto glitch.
- **Caos**: cuántos golpes se rompen (troceo y reordenado, reversa, repeticiones tipo stutter que se aceleran, nubes granulares, silencios).
- **Textura**: qué tan sucio y abstracto suena (bitcrush, reducción de muestreo, granos más cortos, modulación metálica).
- **Volumen**: salida general, con limitador suave para no saturar.

**Crossfader (abajo)**: a la izquierda solo A, a la derecha solo B, en el centro los dos completos. Doble clic lo devuelve al centro.

## Obtener el plugin (compilación automática en GitHub)

Cada vez que se suben cambios a este repositorio, GitHub compila el plugin solo.

1. Entra a la pestaña **Actions** del repositorio.
2. Abre la ejecución más reciente de **Compilar para Mac** (espera a que tenga ✅ verde, tarda unos 10 minutos).
3. Abajo, en **Artifacts**, descarga **CumbiaGlitch-Mac** (un .zip) y descomprímelo.

## Instalar en el Mac

1. En la carpeta descomprimida, haz **clic derecho en `instalar.command` → Abrir → Abrir**. (La primera vez el Mac avisa de que no es de un desarrollador registrado; con clic derecho → Abrir te deja continuar.)
2. Abre Ableton Live. Si no aparece, ve a **Preferencias → Plug-ins**, activa **Usar plug-ins de sistema Audio Units** y **VST3 de sistema**, y pulsa **Reescanear**.
3. Búscalo en el navegador de Ableton como **CumbiaGlitch** (en Plug-ins → Ricardo) y arrástralo a una pista MIDI.

Instalación manual alternativa (Terminal):

```bash
cp -R CumbiaGlitch.vst3 ~/Library/Audio/Plug-Ins/VST3/
cp -R CumbiaGlitch.component ~/Library/Audio/Plug-Ins/Components/
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/CumbiaGlitch.vst3 ~/Library/Audio/Plug-Ins/Components/CumbiaGlitch.component
```

## Notas

- Ableton guarda la ruta de los samples con el proyecto. Si mueves los archivos de audio, tendrás que volver a cargarlos.
- En modo Loop el plugin solo suena con el transporte de Ableton en play.

## Estructura del código

- `Source/PluginProcessor.*`: reproducción de samples, filtros, crossfader, modos Loop/MIDI.
- `Source/GlitchEngine.*`: el motor de efectos glitch.
- `Source/PluginEditor.*`: la interfaz.
- `Tests/RenderTest.cpp`: prueba de audio offline (`cmake -B build -DCUMBIA_BUILD_TESTS=ON`).
- `.github/workflows/build-mac.yml`: compilación automática para Mac.

Hecho con [JUCE](https://juce.com) (licencia AGPLv3 para uso no comercial).
