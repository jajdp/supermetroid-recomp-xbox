# Cómo funciona

*(English: [HOW-IT-WORKS.md](HOW-IT-WORKS.md))*

Qué hace el port de verdad, por qué cada pieza es como es, y qué se midió. Si solo vas a leer un
apartado, lee [el último](#cómo-se-verificó-esto): es lo que da derecho a decir que la receta
funciona.

## La forma del problema

Un proceso UWP en una Xbox no es un proceso de escritorio con menos privilegios. Tres diferencias
deciden el port entero:

1. **No arranca por `main()`.** El sistema arranca un `CoreApplication`, y la ventana es una
   `CoreWindow`, no un `HWND`.
2. **Corre dentro de un AppContainer.** `user32`, `comdlg32`, `dbghelp` y `tlhelp32` no están. El
   código que solo *importa* uno de ellos da un paquete que se instala y luego no activa.
3. **OpenGL no está permitido**, y el renderizador de Direct3D 11 de SDL pide un `HWND` que no
   puede conseguir.

El juego, en cambio, es un `main.c` de unas 2 300 líneas construido sobre SDL, con su propio
renderizador, su audio y su entrada. Así que este port **no** reescribe el host: le pone alrededor
lo que UWP exige y deja al juego hacer lo suyo.

## El presentador

El motor ya tenía la costura buena: un conector `struct RendererFuncs` que el juego rellena con lo
que sea que dibuje el cuadro. La compilación de escritorio enchufa ahí su presentador de OpenGL. El
port enchufa el suyo:

- [`src/uwp_entry.cpp`](../src/uwp_entry.cpp) crea el dispositivo de Direct3D 11 contra la
  `CoreWindow`, sube el cuadro de la PPU emulada como textura y lo dibuja con un par de triángulos a
  pantalla completa.
- [`src/uwp_shaders.hlsl`](../src/uwp_shaders.hlsl) es ese pase; `uwp_shaders.h` es el mismo
  compilado a bytecode, para que la compilación no necesite un compilador de shaders en ejecución.
- El punto de entrada es `WinMain` → `SDL_RunApp` → el main del propio juego. El SDL de UWP ya trae
  dentro el cascarón de `CoreApplication`, así que esto son tres líneas y no una reescritura.

El CRT es el **«_app»**, que es contra el que enlaza `vcvarsall.bat x64 store`, y el ejecutable lleva
el bit **`APPCONTAINER`**, que el generador Ninja de WindowsStore no pone solo
(`src/CMakeLists.txt` pasa `/APPCONTAINER` a mano).

## Los parches, uno por uno

`python tools/apply_uwp.py --list` imprime esto mismo.

**[`patches/01-game-src.patch`](../patches/01-game-src.patch)** — 261 líneas añadidas en tres
archivos del juego:

- `src/main.c`: la entrada de UWP, el presentador enchufado en `RendererFuncs` en lugar del de
  OpenGL, el latido que informa del tiempo por etapa, el contador de FPS en pantalla y
  `SM_RAM_DUMP`, un diagnóstico que vuelca los 128 KB de RAM del invitado en los cuadros que elijas
  —que es cómo «el CRC cambió» pasó a ser «cambiaron estos bytes»—.
- `src/post_mortem.c`: fuera `dbghelp` y `tlhelp32`, que no existen en un AppContainer. El archivo ya
  traía un volcado portable en su rama `#else`; en UWP se usa ese camino.
- `src/sm_cpu_infra.c`: `SmOnStateLoaded`, el enganche que hace funcionar la suspensión. Más abajo.

**[`patches/02-engine-snesrecomp.patch`](../patches/02-engine-snesrecomp.patch)** — 39 líneas en
cuatro archivos del motor:

- `snes/interp_bridge.c`: `__attribute__((constructor))` es una extensión de GCC que MSVC rechaza. Lo
  mismo se consigue con un puntero en la sección `.CRT$XCU`, que es lo que el framework ya hace en
  `mod_runtime.h`.
- `runner/src/launcher_picker.c`: el selector de ROM se compila fuera. Arrastra `COMDLG32.dll` a la
  tabla de importación, y con eso basta para que el paquete no active.
- `runner/src/rom_image_verify.c`, `host_report.c`: sin `user32` y sin `MessageBox`; las migas de pan
  van a un archivo de traza en la carpeta de datos de la app, que en una consola es lo único que se
  puede leer después.

Todo va bajo `#if defined(SM_UWP)`, así que tu compilación de escritorio no se toca.

## El panorámico costaba un 8 % de la simulación, y no era emular

Con `EnhancedRenderer=0` y `Aspect=4:3` la consola clavaba 60 fps. Con el renderizador mejorado a
16:9, la **simulación** se caía a 52-55 fps mientras la **presentación** seguía a 58-59 Hz: no se
veían tirones, el juego simplemente iba un 8-10 % lento.

La tentación es culpar al emulador. El latido por etapas decía otra cosa: rasterizar, componer y
presentar costaba unos **10 ms de un cuadro de 14 ms**. Así que la presentación pasó a su propio
hilo: `EndDraw` copia el cuadro terminado a un búfer de entrega (unos 300 KB) y vuelve, y un segundo
hilo lo presenta.

| | Sin el hilo | Con él |
|---|---|---|
| Cuadros simulados por segundo | 52,9 | **58,3** |
| Coste de presentar, por cuadro | 4,26 ms | **0,03 ms** |
| Segundos a ≥59 fps | 84 de 120 | **115 de 130** |

Es una clave, no una recompilación: `PresentThread=0` en `sm-video.ini` vuelve al camino síncrono,
que es como se hizo el A/B. Los cuadros que siguen bajando son **exactamente los mismos** con hilo y
sin él: son del juego (cuánto de él pudo traducir el recompilador por adelantado), no del host.

Dos mediciones más que vale la pena guardar, porque las dos eran verosímiles y las dos eran falsas:

- **El tipo del paquete no influye.** Medido tres veces como `Game` y tres como `App`: la dispersión
  entre corridas del mismo tipo (3,5 fps) es mayor que la distancia entre tipos.
- **El compilador tampoco.** El primer veredicto culpaba a MSVC, y salía de comparar *nuestro árbol
  compilado con MSVC* contra *el binario oficial de PC del proyecto* —dos variables a la vez—.
  Compilando **nuestro mismo árbol** con GCC, la traza sale igual que con MSVC. Un binario ajeno no
  es el control de tu árbol.

## Ningún botón del mando hacía nada

El mapeo estaba bien y el mando estaba muerto. SDL presenta el mando con **índice de jugador 1**
(contando desde cero) y el `OpenOneGamepad()` del juego toma ese índice como primer hueco candidato:
con el jugador 2 habilitado, el único mando caía en el **puerto 2 del SNES**, que en *Super Metroid*
no hace nada.

`EnableGamepad2 = false` en [`config.ini`](../config/config.ini) descarta ese candidato y el mando
cae en el jugador 1. El propio registro de la consola lo confirma con `assigning to player 1`.

## Volver de Home

En modo desarrollador la consola no suspende un juego que pasa al fondo: lo **termina**. Así que no
hay nada que reanudar — el juego tiene que haberse guardado él.

`RtlLoadSnapshot` restaura el blob del SNES, pero el estado de ejecución del lado del **host** no
cabe ahí, y el runner deja un enganche para que el juego lo reconcilie. Sin ese enganche, el estado
se cargaba y el juego aparecía en la introducción, porque `g_cpu.ram` seguía apuntando donde
apuntara antes. `SmOnStateLoaded` (en `01-game-src.patch`) lo reapunta a la WRAM restaurada,
resincroniza el reloj de la APU y limpia el plazo del intérprete.

## Los finales de línea

El `src/main.c` de `v0.3.1` tiene **3 líneas que acaban en CRLF** de 2 347 — y `01-game-src.patch`
es justo el fragmento que las quita. Por eso el `.gitattributes` mantiene los parches fuera de la
maquinaria de finales de línea (`*.patch -text`): un clon recibe los bytes exactos que produjo
`git diff`.

Medido, porque la historia evidente resultó ser falsa de una manera interesante: una copia del
parche aplanada a LF **también aplica**, y produce un `main.c` idéntico byte a byte (0 CR, 2 577 LF,
100 854 bytes). Pero `git apply --reverse` de esa copia aplanada **no** restaura los tres CR: deja el
archivo 3 bytes más corto que el de upstream, y entonces el parche de verdad ya no le aplica. La
tolerancia no es simétrica, así que los bytes se dejan como están.

## Cómo se verificó esto

No leyéndolo. Sobre un **clon nuevo de `v0.3.1`**, el 2026-10-14:

| Qué se midió | Resultado |
|---|---|
| Los parches aplican a una copia limpia | Los dos, limpios. Primero `--check`, y si algo falla no se escribe nada |
| Idempotencia | Segunda corrida: `already applied`, no escribe nada |
| Los fuentes generados que produce esta receta, contra el árbol del que salió el paquete instalado | **54 de 54 idénticos**, byte a byte (`864 AOT-eligible, 142 LLE-only`, como imprime upstream) |
| Los fuentes parcheados, contra ese mismo árbol | **7 de 7 idénticos**, byte a byte |
| ¿Compila? | Sí: 115 objetos, enlaza `SuperMetroidRecomp.exe` (~3,3 MB), 0 errores |
| ¿Es determinista la compilación? | El mismo árbol compilado desde **dos rutas distintas**: **93 de 93 objetos con `.text` idéntico**, 0 distintos |
| Paquete y firma | `makepri` → `makeappx` → `signtool`, un `.msix` firmado de 6 497 222 bytes |
| Contra el `.msix` que corre en la consola (0.2.0.0) | 1 536 bytes más de `.text` y una cadena más, `FPS readout %s`: una línea de traza que se añadió al árbol después de armar ese paquete. Todo lo demás —el hilo de presentación, los diagnósticos, el mapeo del mando, la suspensión— está en los dos |

Esa última fila es la honesta. La receta reproduce **el árbol actual del proyecto**, que es un poco
más nuevo que el paquete instalado en la consola; no es una divergencia, y está medido en vez de
supuesto.

Y una cosa más que encontró esta verificación, que es la razón de haberla hecho: los parches
guardados en el propio proyecto estaban **desfasados**. Llevaban 121 de las 267 líneas cambiadas
—`main.c` a medias, y `src/sm_cpu_infra.c`, el enganche de la suspensión, nada—. Quien los siguiera
habría construido un juego que pierde la partida cada vez que la consola la reclama. Los parches de
aquí se regeneraron del árbol que de verdad corre, que es por lo que se extraen con `git diff` y
nunca se transcriben a mano.
