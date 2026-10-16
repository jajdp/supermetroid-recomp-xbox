# Los mosaicos del paquete — los trece archivos que pones tú

*(English: [ASSETS.md](ASSETS.md))*

Este repositorio **no lleva arte**. Los mosaicos de la consola de verdad se hicieron con el arte de
*Super Metroid*, que es de Nintendo, así que no están aquí y `package/Assets/` está vacía a
propósito ([`../NOTICE.es.md`](../NOTICE.es.md)).

Pones trece archivos PNG en una carpeta tuya y la pasas con `--tiles`. `package_uwp.py` comprueba
los trece, por nombre y por cabecera PNG, y se detiene con la lista de lo que falte en vez de armar
un paquete que la consola enseñe sin icono.

## Los trece, con las medidas que se usaron

| Archivo | Medida | Qué hace la consola con él |
|---|---|---|
| `Square150x150Logo.png` | 150×150 | **El mosaico que se ve de verdad** en Mis juegos y aplicaciones |
| `Square150x150Logo.scale-200.png` | 300×300 | El mismo al 200 %, que es el que coge un panel en 4K |
| `SplashScreen.png` | 620×300 | **La pantalla de carga**, la otra imagen que se ve de verdad |
| `SplashScreen.scale-200.png` | 1240×600 | La misma al 200 % |
| `Square44x44Logo.png` | 44×44 | Icono pequeño, en listas |
| `Square44x44Logo.scale-200.png` | 88×88 | El mismo al 200 % |
| `Square44x44Logo.targetsize-24_altform-unplated.png` | 24×24 | Icono pequeño sin placa detrás |
| `StoreLogo.png` | 50×50 | El `<Logo>` del manifiesto |
| `StoreLogo.scale-200.png` | 100×100 | El mismo al 200 % |
| `LockScreenLogo.png` | 24×24 | Se declara por completitud; una consola no lo usa |
| `LockScreenLogo.scale-200.png` | 48×48 | Ídem |
| `Wide310x150Logo.png` | 310×150 | El mosaico ancho: se declara en el manifiesto y la consola **no lo dibuja** |
| `Wide310x150Logo.scale-200.png` | 620×300 | Ídem |

Solo dos importan a la vista: `Square150x150Logo` y `SplashScreen`. Los demás existen porque el
manifiesto los nombra y el empaquetado los indexa.

## Cómo hacerlos

Con cualquier editor de imágenes. Tres cosas que es fácil equivocar:

- **PNG, con transparencia de verdad donde la quieras.** El `BackgroundColor` del manifiesto es
  `transparent`, así que por donde tu imagen sea transparente se ve el panel de la consola.
- **Los `.scale-200` son otra imagen al doble de tamaño**, no el mismo archivo renombrado.
  `makepri` los indexa como alternativas y la consola elige según la pantalla.
- **Las medidas exactas.** La consola no te las reescala: una medida rara sale recortada o con
  márgenes.

No les metas EXIF ni fragmentos de texto. Un PNG exportado por una herramienta de capturas puede
llevar dentro una ruta o el nombre de una máquina, que es una cosa rara que publicar junto a una
imagen.
