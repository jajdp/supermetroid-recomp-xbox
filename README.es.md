# Super Metroid en la Xbox — el port UWP

Cómo coger la recompilación estática [SuperMetroidRecomp](https://github.com/mstan/SuperMetroidRecomp)
de *Super Metroid* (Super Nintendo, 1994) y hacerla correr **en una Xbox Series X en modo
desarrollador**, como un paquete UWP que compilas y firmas tú.

*(English: [README.md](README.md))*

No es una teoría: el juego arranca, se juega con mando en **16:9**, mantiene la simulación a **58,3
cuadros por segundo** presentando a 59 Hz, enseña su cadencia en pantalla al pulsar el stick derecho
y vuelve donde lo dejaste después de que la consola lo termine.

## En qué consola se probó

**En una Xbox Series X**, en modo desarrollador. Las dos consolas en las que se construyó y se probó esto
son Series X, así que es el único hardware al que se refiere cualquier cosa que se afirme aquí.

**En una Series S no se ha probado nunca, ni una vez.** Es otra máquina: menos memoria disponible para un
juego y una GPU más floja. Puede que funcione en una, puede que vaya más despacio, o puede que no
arranque; nada de aquí se ha medido en ella. Si lo ejecutas en una Series S, lo que obtengas no es lo que
dice este README, y ninguna de las cifras de aquí es una promesa sobre esa consola.

## Qué es este repositorio

**Una receta y las herramientas que la aplican.** Nadie había publicado una versión UWP de este
juego, así que el port es nuestro: un presentador de Direct3D 11 que dibuja sobre la CoreWindow de
la consola, un punto de entrada de WinRT y los cambios que el motor necesitaba para sobrevivir
dentro de un AppContainer. Dos parches, cuatro archivos de código y dos scripts.

Todo se aplica **a tu propia copia** del proyecto del juego, en tu máquina. Cada añadido va bajo
`#if defined(SM_UWP)`, así que una copia parcheada sigue compilando y corriendo en el escritorio
igual que antes.

## Qué **no** es

Aquí **no hay ROM, ni código del juego, ni ejecutable compilado, ni paquete `.msix`, ni arte**. Esto
no puede dar un juego jugable por sí solo, y nada de lo que te puedas descargar aquí es un juego.
Necesitas tu propio volcado legal del cartucho y una compilación de escritorio del proyecto del
juego que ya funcione. Ver [`NOTICE.es.md`](NOTICE.es.md).

Los mosaicos del paquete tampoco están: los de la consola de verdad se hicieron con el arte del
propio juego, que es de Nintendo. Los pones tú — [`docs/ASSETS.es.md`](docs/ASSETS.es.md) lista los
trece archivos y sus medidas exactas, y `package_uwp.py` se detiene con un mensaje claro si falta
alguno.

## Qué hace falta antes

| | |
|---|---|
| El proyecto del juego | [mstan/SuperMetroidRecomp](https://github.com/mstan/SuperMetroidRecomp) `v0.3.1`, clonado con su submódulo `snesrecomp`, **compilando y corriendo antes en tu PC** |
| Tu propia ROM | Un volcado `.sfc` de *Super Metroid (Japan, USA)*. El SHA-1 `da957f0d63d14cb441d215462904c4fa8519c613` es el que se usó aquí |
| Visual Studio | 2022, con las herramientas de C++. Lo que importa es **en qué entorno compilas**: `vcvarsall.bat x64 store`, nunca `vcvars64.bat` ([`docs/DEPENDENCIES.es.md`](docs/DEPENDENCIES.es.md)) |
| SDK de Windows | Cualquiera reciente de 10/11; `package_uwp.py` busca el más nuevo él solo |
| CMake y Ninja | Los dos vienen con Visual Studio 2022 |
| Rust (`cargo`), `bash`, Python 3 | El analizador del recompilador es código nativo y su script de regeneración es de shell. De Python solo se usa la biblioteca estándar |
| SDL3 compilado para UWP | El SDL3 de escritorio no sirve. En [`docs/DEPENDENCIES.es.md`](docs/DEPENDENCIES.es.md) está qué tiene que ser |
| Una Xbox en modo desarrollador | Y un certificado para firmar — uno autofirmado vale ([`docs/INSTALL.es.md`](docs/INSTALL.es.md) apartado 4) |

## Cómo va

Cuatro pasos, y la versión larga de cada uno está en [`docs/INSTALL.es.md`](docs/INSTALL.es.md).

> **Dónde se escribe esto:** en **PowerShell** — pulsa la tecla **Windows**, escribe `powershell` y
> pulsa **Entrar** — en una ventana **normal**: no hace falta «ejecutar como administrador», y usarlo
> rompería el paso de la firma, porque el certificado vive en el almacén de quien ejecuta la orden.
> Desde la carpeta de este repositorio.

```powershell
# 1. generar la recompilación en tu copia (en bash, y con el backend de análisis NATIVO)
#    Ver docs/INSTALL.es.md apartado 2: el backend de python da un juego que va a 4 fps.

# 2. parchear tu copia (idempotente, y comprueba antes de escribir nada)
python tools\apply_uwp.py <ruta-a-tu-copia>

# 3. configurar y compilar, en una ventana donde hayas cargado `vcvarsall.bat x64 store`
cmake -S src -B <ruta-de-build-corta> -G Ninja -DCMAKE_SYSTEM_NAME=WindowsStore ^
      -DCMAKE_SYSTEM_VERSION=10.0 -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
      -DSM_SRC=<ruta-a-tu-copia> -DSDL3_UWP=<ruta-al-sdl3-de-uwp>
cmake --build <ruta-de-build-corta> --target sm_uwp

# 4. empaquetar y firmar (tus mosaicos, tu certificado)
python tools\package_uwp.py --exe <ruta-de-build-corta>\SuperMetroidRecomp.exe ^
       --rom "<tu rom>.sfc" --extra <ruta-al-sdl3-de-uwp>\bin\SDL3.dll ^
       --tiles <tu carpeta de mosaicos> --certificate <tu huella> --work <carpeta de trabajo>
```

`python tools\apply_uwp.py --list` imprime los dos parches y qué resuelve cada uno.

Después se instala el `.msix` por el Device Portal y se copian los dos archivos de ajustes de
[`config/`](config/) al `LocalState` de la app. Paso a paso, incluido el lado de la consola:
**[`docs/INSTALL.es.md`](docs/INSTALL.es.md)**.

## Qué hubo que resolver

Cada una de estas cosas fue un fallo real en la consola, y la mayoría escondía la siguiente. El
detalle, con lo que decía el registro en cada caso y lo que se midió, está en
**[`docs/HOW-IT-WORKS.es.md`](docs/HOW-IT-WORKS.es.md)**.

| | Síntoma | Arreglo |
|---|---|---|
| 1 | `SDL_CreateRenderer` fallaba: «Couldn't get window handle» | El backend D3D11 de SDL pide un HWND, y en UWP hay una CoreWindow. El motor ya tenía un conector de presentadores (`RendererFuncs`), así que el port enchufa ahí su propio presentador de Direct3D 11 |
| 2 | La consola instalaba el paquete y luego no lo arrancaba (`0x8027025B`) | Dos causas distintas con el mismo error: el ejecutable importaba el CRT **de escritorio** y, más tarde, que `SDL3.dll` no iba dentro del paquete. Las dos se comprueban ahora antes de firmar |
| 3 | La consola rechazaba el paquete al activarlo | Faltaba el bit `APPCONTAINER`. El generador Ninja de WindowsStore no lo pone solo |
| 4 | **~4 fps**, y el juego no pasaba de la intro | La recompilación se había generado con el backend de análisis `python`: emite 1 807 variantes AOT contra 864 del nativo, y al menos una está mal generada — corrompe la pila del invitado. **El compilador no tenía nada que ver**, y averiguarlo costó un desvío |
| 5 | Pulsar **B** cerraba el juego | En la Xbox, B es también el «atrás» del sistema, y el shell se lleva la app por delante si no dice que ha atendido la tecla |
| 6 | El juego se iba al selector de ROM en la consola | Reancla su directorio de trabajo al del ejecutable, que en un paquete instalado es de solo lectura. Se lanza con `--config <ruta absoluta>` y la ROM como argumento posicional |
| 7 | El selector de ROM arrastraba `COMDLG32.dll` a la tabla de importación | Ese DLL no existe dentro de un AppContainer. El selector se compila fuera |
| 8 | `__attribute__((constructor))` no compilaba | Es una extensión de GCC. Lo mismo se hace con un puntero en la sección `.CRT$XCU`, que es lo que el propio framework ya hace en otro sitio |
| 9 | El volcado post mortem pedía `dbghelp` y `tlhelp32` | Ninguno existe dentro de un AppContainer. El archivo ya traía una alternativa portable; en UWP se usa esa |
| 10 | **Ningún** botón del mando hacía nada | SDL presenta el mando con índice de jugador 1 (base 0) y el juego toma ese índice como primer hueco libre, así que el único mando caía en el puerto 2 del SNES. `EnableGamepad2 = false` es lo que hace que el mando responda — ver [`config/config.ini`](config/config.ini) |
| 11 | El 16:9 costaba un 8-10 % de la simulación (52,9 fps) | Rasterizar, componer y presentar costaba ~10 ms de los 14 de un cuadro; emular no era el problema. La presentación pasó a su propio hilo: **58,3 fps**, y presentar bajó de 4,26 ms a 0,03 |
| 12 | No había forma de saber la cadencia | Una consola no tiene barra de título, que es donde el runtime la escribe. Un contador en pantalla, que se enciende con **R3** |
| 13 | Volver de Home empezaba el juego de nuevo | En modo desarrollador la consola *termina* el juego suspendido. El estado se guarda mientras corre y se recarga cuando el sistema lo relanza, lo que necesita un enganche de reconciliación del lado del host (`src/sm_cpu_infra.c`) que el motor deja al juego |

## Cómo se verificó

No leyéndolo: reconstruyéndolo. Sobre un clon limpio de `v0.3.1`, esta receta reproduce el árbol del
que salió el paquete instalado — **7 de 7 archivos idénticos byte a byte**, medido, incluido el
archivo que a los parches guardados del propio proyecto les faltaba. Las cifras y el método están en
[`docs/HOW-IT-WORKS.es.md`](docs/HOW-IT-WORKS.es.md).

## Créditos y licencia

Port y herramientas de **Recompilaciones**. Publicado bajo la
[PolyForm Noncommercial License 1.0.0](LICENSE), la misma licencia del proyecto SuperMetroidRecomp y
del motor snesrecomp, de los que esto es un derivado.

Los parches llevan sus comentarios en español, que es donde se escribieron; la documentación está en
los dos idiomas.

*Super Metroid* es © Nintendo. Este proyecto no está afiliado con ellos, ni con Microsoft, ni con
los autores del proyecto del juego o del motor, y no distribuye nada que les pertenezca. El
detalle —qué está incluido aquí exactamente y qué no, y cómo pedir una retirada— está en
[`NOTICE.es.md`](NOTICE.es.md). Los titulares de derechos pueden escribir a **jajdpmail@gmail.com**.
