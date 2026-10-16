# Instalación — de una copia limpia al juego corriendo en la consola

*(English: [INSTALL.md](INSTALL.md))*

Ocho apartados. Del 1 al 5 pasan en tu PC; el 6 y el 7, en la consola. Todo esto se ejecutó de
principio a fin sobre un clon nuevo antes de escribirlo; donde un paso tiene trampa, la trampa está
en el paso y no en una nota al pie.

## 0. Dónde se escriben estas órdenes

Dos clases de ventana, y sí importa cuál:

- **PowerShell** — pulsa la tecla **Windows**, escribe `powershell` y pulsa **Entrar**. Para los
  scripts de Python, el certificado y el Device Portal.
- **Una ventana de `cmd` con el entorno de compilación cargado** — pulsa **Windows**, escribe `cmd`,
  **Entrar**, y ejecuta dentro la línea de `vcvarsall.bat` del apartado 3. Para CMake y Ninja. El
  entorno solo dura lo que dure esa ventana.
- **Git Bash** — se instala con Git para Windows. Se usa una vez, para el script de regeneración del
  proyecto, en el apartado 2, que es de shell.

**Para nada de esto hace falta «ejecutar como administrador»**, y usarlo rompería justo el apartado
4: el certificado se crea en el almacén de quien ejecuta la orden, así que una ventana elevada lo
dejaría donde el paso de la firma no lo encuentra.

Las órdenes que ejecutan un `.ps1` llevan `-ExecutionPolicy Bypass`. Un Windows de fábrica se niega
a ejecutar archivos de script (`Restricted`), y ese modificador vale solo para ese proceso: no
cambia nada de tu sistema y no pide administrador.

## 1. Clonar el proyecto del juego

```powershell
git clone --branch v0.3.1 https://github.com/mstan/SuperMetroidRecomp.git C:\smr
cd C:\smr
git submodule update --init --depth 1 snesrecomp
git submodule update --init --depth 1 recomp-ui
```

Tres cosas que conviene saber, y cada una cuesta una hora si la descubres por las malas:

- **La ruta, corta** (`C:\smr`, no una carpeta en lo hondo de Documentos). El analizador del
  recompilador es un binario de Rust que con una ruta larga choca con `MAX_PATH` y falla al enlazar
  con `LNK1104`.
- **`recomp-ui` tiene que estar aunque nada se enlace contra él.** `src/sm_mods.h` incluye
  `recomp_launcher.h` solo por sus tipos. Sin ese submódulo la compilación se para con
  `fatal error C1083: Cannot open include file: 'recomp_launcher.h'`. La compilación de escritorio
  del juego no lo necesita, y por eso se pasa por alto.
- No uses `--recurse-submodules`: hay un submódulo anidado (`retcomm-rbengine`) sin URL en
  `.gitmodules` y git se queja. Los dos de arriba es todo lo que esta receta necesita.

Después pon tu propia ROM en la raíz de la copia, con el nombre exacto que espera el proyecto:

```
C:\smr\Super Metroid (Japan, USA) (En,Ja).sfc
```

Su SHA-1 debería ser `da957f0d63d14cb441d215462904c4fa8519c613`
([`DEPENDENCIES.es.md`](DEPENDENCIES.es.md)).

## 2. Generar la recompilación

En **Git Bash**, desde la raíz de la copia:

```bash
PYTHON="C:/Users/<tú>/AppData/Local/Programs/Python/Python313/python.exe" \
SNESRECOMP_ANALYSIS_BACKEND=native \
bash tools/regen.sh --no-tests
```

`PYTHON` tiene que apuntar a un Python **de verdad**. El `python3` que Windows pone en el PATH por
defecto es un cascarón que abre la Microsoft Store, y el script falla de una manera que no lo dice.

⚠ **`SNESRECOMP_ANALYSIS_BACKEND=native` no es opcional.** Es el valor por defecto del proyecto, así
que basta con no pisarlo; pero si corre el backend `python`, lo que sale es una compilación que
compila, instala, arranca y luego se queda a **4 cuadros por segundo** sin pasar de la intro. Emite
1 807 variantes AOT contra 864 del nativo, y al menos una está mal generada: corrompe la pila del
invitado. Nada en el síntoma apunta a la causa, y con esa cadencia sospechas primero de la consola,
del compilador y del presentador.

Cómo acaba una corrida correcta:

```
analysis: 864 AOT-eligible, 142 LLE-only
v2_emit: 218 roots, 864 exact AOT variants, 142 LLE variants
v2_emit: 52 bank(s) emitted, 0 reused
```

y **54 archivos `.c`** en `src/gen`. Si ves otros números, párate aquí: el resto de la receta va a
construir algo, y ese algo no va a funcionar bien.

## 3. Parchear y compilar

Desde la carpeta de este repositorio, en **PowerShell**:

```powershell
python tools\apply_uwp.py C:\smr
```

Comprueba los dos parches contra tu copia antes de escribir nada, y se niega a aplicar uno si el
otro no entra: quedarse a medio parchear es el estado del que de verdad hay que protegerse. Córrelo
dos veces y la segunda dice `already applied` y no hace nada. `--check` informa sin escribir,
`--revert` lo deshace y `--list` imprime qué cambia cada parche y por qué.

Después, en una ventana de **`cmd`**, carga el entorno y compila:

```
"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 store
```

```
cmake -S "<este repo>\src" -B C:\smu -G Ninja ^
  -DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0 ^
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL ^
  -DSM_SRC=C:/smr -DSDL3_UWP=<ruta a tu SDL3 de UWP>/x64

cmake --build C:\smu --target sm_uwp
```

- **`x64 store`, no `vcvars64`.** Es lo que enlaza contra el CRT «_app». Con el de escritorio el
  paquete se instala y luego la consola dice *«the app didn't start»* (`0x8027025B`).
- **`-DSM_SRC` y `-DSDL3_UWP` van con barras normales**, incluso en Windows.
- **La ruta del build, también corta** (`C:\smu`), por el mismo `MAX_PATH`.
- Lo que sale es `C:\smu\SuperMetroidRecomp.exe`, unos 3,3 MB.

## 4. Un certificado para firmar

Una orden, en **PowerShell**, en una ventana **normal** (no de administrador):

```powershell
New-SelfSignedCertificate -Type Custom -Subject "CN=<tu nombre>" -KeyUsage DigitalSignature `
  -FriendlyName "<tu nombre> UWP dev" -CertStoreLocation "Cert:\CurrentUser\My" `
  -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3", "2.5.29.19={text}")
```

Apunta el `Thumbprint` que imprime. Luego abre
[`../package/Package.appxmanifest`](../package/Package.appxmanifest) y cambia los dos valores
`CHANGE-ME`:

- `Publisher` tiene que ser **exactamente** el `-Subject` de arriba, letra por letra y con el `CN=`.
  La consola compara las dos cadenas y rechaza el paquete si no coinciden; `package_uwp.py` te lo
  comprueba antes de empaquetar nada.
- `Name` es el nombre interno del paquete. Con el Publisher forma la **familia**, que es como la
  consola reconoce el juego y de donde cuelga su `LocalState` —los ajustes y las partidas—.
  Cambiarlo más adelante crea un segundo juego a ojos de la consola, con la carpeta en blanco, y
  deja el viejo instalado aparte. Elígelo una vez.

Un certificado autofirmado vale: una consola en modo desarrollador instala paquetes firmados con uno.

## 5. Empaquetar, firmar e instalar

Primero tus mosaicos: este repositorio no trae ninguno, porque los de la consola de verdad se
hicieron con el arte del juego. En [`ASSETS.es.md`](ASSETS.es.md) están los trece archivos y sus
medidas exactas.

```powershell
python tools\package_uwp.py --exe C:\smu\SuperMetroidRecomp.exe ^
  --rom "C:\smr\Super Metroid (Japan, USA) (En,Ja).sfc" ^
  --extra <ruta a tu SDL3 de UWP>\x64\bin\SDL3.dll ^
  --tiles <tu carpeta de mosaicos> --certificate <tu huella> ^
  --version 0.1.0.0 --work C:\smu_pkg
```

Antes de empaquetar nada comprueba las cinco cosas que separan un paquete que se instala de un
paquete que arranca: el bit `APPCONTAINER`, el CRT «_app», que todo DLL importado esté en el paquete
o en la consola, el `Publisher` contra el certificado, y los trece mosaicos. El resultado es un
`.msix` firmado de unos 6,5 MB en `--work`.

Instálalo desde el **Device Portal** de la consola (`https://<ip de la consola>:11443`, usuario
`xbox`): **Add** → el `.msix` → y, como dependencia, el appx de `Microsoft.VCLibs.140.00` para x64
que viene con el SDK de Windows. La consola necesita **un usuario conectado**; sin nadie conectado,
el paquete se instala y al abrirlo no pasa nada.

## 6. Los dos archivos de ajustes

**No** viajan dentro del paquete. Viven en la carpeta de datos de la app y sobreviven a
reinstalarla. Copia los dos de [`../config/`](../config/) a:

```
\LocalState\SuperMetroidRecomp\config.ini
\LocalState\SuperMetroidRecomp\sm-video.ini
```

con el explorador de archivos del Device Portal, y luego para y relanza la app para que los lea.

- [`config.ini`](../config/config.ini) es el audio y el mapeo del mando. **`EnableGamepad2 = false`
  es lo que hace que el mando responda** — el motivo está en el propio archivo y en
  [`HOW-IT-WORKS.es.md`](HOW-IT-WORKS.es.md).
- [`sm-video.ini`](../config/sm-video.ini) es el panorámico y el hilo de presentación.
  `PresentThread=1` vale 5 cuadros por segundo; déjalo en 1.

⚠ El escritor de ajustes del propio juego solo conoce sus cinco claves, así que si alguna vez
reescribe ese archivo **se lleva por delante la línea `PresentThread`**. No pasa nada: el valor por
defecto del código es el bueno.

## 7. Jugarlo

Conecta el mando **antes** de lanzarlo, o el juego no lo verá (`No joysticks detected`). Pulsa el
**stick derecho (R3)** para el contador de cuadros en pantalla.

Volver de Home funciona: en modo desarrollador la consola termina el juego suspendido, así que el
port guarda su estado mientras corre y lo recarga cuando el sistema lo relanza.

## 8. Cuando algo sale mal

| Lo que ves | Qué es |
|---|---|
| `fatal error C1083: ... 'recomp_launcher.h'` | El submódulo `recomp-ui` no está inicializado (apartado 1) |
| `LNK1104` al enlazar | La copia o la ruta del build son demasiado largas (`MAX_PATH`) |
| *«the app didn't start»* / `0x8027025B` | O el CRT de escritorio (compilaste con `vcvars64`) o falta un DLL en el paquete. `package_uwp.py` caza los dos antes de firmar |
| La consola rechaza el paquete al instalar | El `Publisher` y el sujeto del certificado no son la misma cadena (apartado 4) |
| La consola lo rechaza al activarlo | Falta el bit `APPCONTAINER`: una compilación que no salió de `src/CMakeLists.txt` |
| **4 fps** y no pasa de la intro | La recompilación se generó con el backend de análisis `python` (apartado 2) |
| Corre, pero **ningún** botón del mando hace nada | Falta `EnableGamepad2 = false` en el `config.ini` (apartado 6) |
| Se va a un selector de ROM | La ROM no está en el paquete con el nombre que espera el `main.c` parcheado (`sm.sfc` por defecto; `--rom-name` para cambiarlo) |
| Pulsar **B** cierra el juego | Estás ejecutando algo compilado sin `01-game-src.patch` |
| Los ajustes «volvieron» tras reinstalar | El `LocalState` sobrevive a la reinstalación. Repón los dos archivos y relanza |
| `No certificate with thumbprint ...` cuando está ahí claramente | Tu `PSModulePath` está reescrito, normalmente por una instalación de PowerShell 7, y en la 5.1 ya no existe el disco `Cert:`. `package_uwp.py` lee el almacén por .NET justo por esto, así que si lo ves desde *otro* script, esa es la causa |
