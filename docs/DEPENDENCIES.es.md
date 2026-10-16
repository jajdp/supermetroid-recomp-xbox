# Dependencias

*(English: [DEPENDENCIES.md](DEPENDENCIES.md))*

Todo lo de aquí está medido en la máquina donde se hizo el port. Donde la versión importa, se dice
por qué; y donde no importa, también, para que no andes buscando un número exacto que da igual.

## El proyecto del juego

| | |
|---|---|
| Repositorio | [mstan/SuperMetroidRecomp](https://github.com/mstan/SuperMetroidRecomp) |
| Etiqueta | **`v0.3.1`** — los parches se generaron contra esta etiqueta. En una posterior puede que sigan aplicando, y `apply_uwp.py` te lo dice antes de escribir nada en vez de dejarte el árbol a medio parchear |
| Submódulo `snesrecomp` | Commit `01f3c73`, el que fija `v0.3.1`. Es el motor, y `02-engine-snesrecomp.patch` se aplica **dentro** de él |
| Submódulo `recomp-ui` | Commit `76392fb`. **Hay que inicializarlo aunque nada se enlace contra él:** `src/sm_mods.h` incluye `recomp_launcher.h` solo por sus tipos, y sin el submódulo la compilación se para con `fatal error C1083: Cannot open include file: 'recomp_launcher.h'`. Es el paso que se olvida, porque la compilación de escritorio del juego no lo necesita |

```powershell
git clone --branch v0.3.1 https://github.com/mstan/SuperMetroidRecomp.git <copia>
cd <copia>
git submodule update --init --depth 1 snesrecomp
git submodule update --init --depth 1 recomp-ui
```

Hay un submódulo anidado, `retcomm-rbengine`, sin URL en `.gitmodules`; con `--recurse-submodules`
git se queja de él. Inicializando los dos de arriba por su nombre te ahorras el ruido, y esta receta
no necesita ningún otro.

**La ruta, corta.** Poner `<copia>` en algo como `C:\smr` o `D:\juegos\smr` te ahorra una compilación que
falla al enlazar con `LNK1104` sin motivo aparente: el analizador del recompilador es un binario de
Rust y con una ruta larga choca con `MAX_PATH`.

## Tu propia ROM

| | |
|---|---|
| Archivo | Un volcado `.sfc` de *Super Metroid (Japan, USA) (En,Ja)* |
| SHA-1 | `da957f0d63d14cb441d215462904c4fa8519c613` |
| Tamaño | 3 145 728 bytes |

El volcado tiene que estar en la raíz de tu copia con ese nombre exacto para que lo encuentre el
script de regeneración del propio proyecto. Aquí no se distribuye nada de él; el hash es una huella,
para que puedas saber si tu copia es la que se usó.

## Herramientas de compilación

| | Versión usada | ¿Importa la versión? |
|---|---|---|
| Visual Studio 2022 | Community, MSVC 14.44 | **No**, pero el *entorno* sí: `vcvarsall.bat x64 store`, nunca `vcvars64.bat`. Es lo que enlaza contra el CRT «_app», que es el que tiene la consola. Con el de escritorio el paquete se instala y luego no arranca (`0x8027025B`). La carga de trabajo de C++ para UWP **no hace falta**: aquí empaquetan las herramientas del SDK, no MSBuild |
| SDK de Windows | 10.0.26100.0 | **No.** `package_uwp.py` busca el SDK más nuevo que tenga `makeappx`, `makepri` y `signtool`, y usa ese |
| CMake | 3.20 o posterior | Viene con Visual Studio 2022 |
| Ninja | Cualquiera | Viene con Visual Studio 2022. ⚠ Su generador de WindowsStore **no** pasa `/APPCONTAINER`; `src/CMakeLists.txt` lo pasa a mano, y `package_uwp.py` se niega a empaquetar un ejecutable sin ese bit |
| Python 3 | 3.13 | **No.** Solo biblioteca estándar, nada que instalar. Con 3.8 o posterior vale |
| Rust (`cargo`) | Cualquiera reciente | Hace falta una vez: el backend de análisis del recompilador es un binario de Rust que el proyecto compila por ti |
| `bash` | El de Git para Windows | El script de regeneración del proyecto es de shell |
| PowerShell | La 5.1 que ya trae Windows | **PowerShell 7 no es una dependencia.** Los ejemplos llevan `-ExecutionPolicy Bypass` porque un Windows de fábrica está en `Restricted` y sin eso no ejecuta un `.ps1` |

## SDL3, compilado para UWP

Esta es la dependencia que todo el mundo equivoca, porque un SDL3 de escritorio parece que debería
servir.

| | |
|---|---|
| Qué tiene que ser | Un SDL3 compilado para WindowsStore: con el bit `APPCONTAINER`, el CRT «_app» y el backend de vídeo de WinRT. Los binarios de escritorio de SDL no son nada de eso |
| Qué necesitas de él | `lib/SDL3.lib` para enlazar, `include/` para compilar y `bin/SDL3.dll` **dentro del paquete** — este juego enlaza SDL de forma dinámica |
| Versión usada aquí | SDL 3.x compilado desde el código con el entorno `store`. El `SDL3.dll` que viajó mide 1 893 888 bytes |

Si falta en el paquete, la consola lo instala encantada y luego no arranca con el **mismo**
`0x8027025B` que da un CRT equivocado, que es lo que lo vuelve tan confuso. `package_uwp.py` lee las
importaciones del ejecutable y se para si algún DLL que no está en la consola tampoco está en el
paquete.

## La consola

| | |
|---|---|
| Hardware | Xbox Series X o Series S |
| Modo | **Modo desarrollador**, con Dev Home instalado |
| Acceso | El Device Portal por la red, y **un usuario conectado** en la consola — sin nadie conectado, el paquete se instala y al abrirlo no pasa nada |
| Dependencia en la consola | `Microsoft.VCLibs.140.00` desde la 14.0.33519.0. Se declara en el manifiesto y se instala junto al paquete, no va dentro |
