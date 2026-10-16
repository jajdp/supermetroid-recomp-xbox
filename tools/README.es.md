# tools/

*(English: [README.md](README.md))*

Dos scripts. Los dos son Python 3 pelado, sin nada más que la biblioteca estándar; los dos reciben
cada ruta como argumento —aquí no hay ningún valor por defecto que apunte a la máquina de nadie— y
los dos comprueban antes de escribir.

| Script | Qué hace |
|---|---|
| `apply_uwp.py` | Aplica los dos parches de este repositorio a **tu** copia del proyecto del juego. `--check` informa sin escribir, `--revert` lo deshace, `--list` explica cada parche. Idempotente: a la segunda dice `already applied` |
| `package_uwp.py` | Convierte lo que compilaste en un `.msix` firmado: `makepri` → `makeappx` → `signtool`. Busca el SDK de Windows él solo |

```powershell
python tools\apply_uwp.py <tu copia>
python tools\package_uwp.py --exe <build>\SuperMetroidRecomp.exe --rom "<tu rom>.sfc" ^
    --extra <sdl3 de uwp>\bin\SDL3.dll --tiles <tus mosaicos> --certificate <huella> ^
    --work <carpeta de trabajo>
```

La huella del certificado puede venir de la variable de entorno `UWP_CERT_THUMBPRINT` en vez de la
línea de órdenes, para que no acabe en el historial de la consola.

## Qué comprueba `package_uwp.py` antes de empaquetar nada

Esas comprobaciones son el sentido del script. Cada una vale por un día perdido con una consola que
instala el paquete encantada y luego no lo arranca:

1. el bit **`APPCONTAINER`** del ejecutable;
2. el **CRT «_app»**, que es lo que distingue una compilación con `vcvarsall x64 store` de una con
   `vcvars64`;
3. **todos los DLL que importa el ejecutable**, o dentro del paquete o de los que trae la consola
   —SDL3 es el que muerde, porque este juego lo enlaza de forma dinámica—;
4. el **`Publisher`** del manifiesto contra el sujeto del certificado que va a firmar, comparados
   como cadenas, que es como los compara la consola;
5. los **trece mosaicos**, por nombre y por cabecera PNG.

No escribe nada fuera de `--work`, y rehace el `content/` de esa carpeta en cada corrida para que un
resto de un intento anterior no pueda acabar dentro de un paquete firmado.

## Sobre el idioma

Estos dos scripts están en inglés, comentarios incluidos. Lo que está en español es el código **de
dentro de los parches**: los comentarios que entran en el árbol del juego se escribieron ahí y se
publican tal como se compilaron y se verificaron, en vez de volver a teclearlos para publicarlos.
