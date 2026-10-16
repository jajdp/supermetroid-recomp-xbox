# Aviso

*(English: [NOTICE.md](NOTICE.md))*

## Sin afiliación

Este es un proyecto de aficionado, no oficial y sin ánimo de lucro. **No está afiliado,
patrocinado ni relacionado con Nintendo ni con Microsoft**, ni con los autores de
[SuperMetroidRecomp](https://github.com/mstan/SuperMetroidRecomp),
[snesrecomp](https://github.com/mstan/snesrecomp) o [SDL](https://github.com/libsdl-org/SDL).
*Super Metroid*, *Xbox* y todos los nombres y marcas relacionados son de sus respectivos
propietarios.

Aquí no se vende nada y nada está monetizado.

## Qué NO se distribuye en este repositorio

Ninguna ROM. Ningún código del juego de Nintendo. Ningún fuente recompilado. Ningún ejecutable
compilado. Ningún paquete `.msix`. Ninguna partida guardada. Ningún mosaico del paquete —esos se
hicieron con el arte del propio juego—. Ningún certificado de firma, ni la huella de uno.

El `.gitignore` bloquea todo eso por extensión, para que no pueda colarse por descuido. Es una
red de seguridad, no un sustituto de leer este archivo.

## Qué contiene de trabajo ajeno

Medido archivo por archivo, para que «no distribuimos nada suyo» sea algo que puedas comprobar y
no algo que tengas que creerte:

| | Líneas | Qué es |
|---|---|---|
| Código propio | 937 | `src/uwp_entry.cpp`, `src/uwp_shaders.h`, `src/uwp_shaders.hlsl` y `src/CMakeLists.txt`: el presentador de Direct3D 11, el punto de entrada de WinRT y el objetivo de compilación. Escrito aquí |
| Herramientas propias | 422 | `tools/apply_uwp.py` y `tools/package_uwp.py` |
| Líneas propias dentro de los parches | 300 | Lo que los parches **añaden** a tu copia, todo bajo `#if defined(SM_UWP)` |
| Citado de SuperMetroidRecomp y snesrecomp | 183 | Las **líneas de contexto** que un diff unificado tiene que llevar para saber dónde va cada cambio, más 9 líneas que los parches quitan. Nada más suyo está aquí |

Esas 183 líneas se citan de proyectos licenciados bajo la
[PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0),
que permite cambios y obras derivadas con fines no comerciales, y este repositorio se publica bajo
esa misma licencia para que encaje. Su línea `Required Notice:` está en [`LICENSE`](LICENSE).

La ROM, y solo la ROM, la pones tú, y nunca pasa por aquí. Lo que la receta sí lleva sobre ella es
un **hash SHA-1** —una huella, no contenido— para que puedas saber si el volcado que tienes es el
que se usó aquí.

## Retirada

Si tienes derechos sobre algo de lo anterior y quieres que se retire, escribe a
**jajdpmail@gmail.com** y se retira. Sin discusión y sin demora.
