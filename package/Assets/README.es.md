# Esta carpeta está vacía a propósito

*(English: [README.md](README.md))*

Los trece mosaicos del paquete **no** están en este repositorio. Los que se usaron en la consola de
verdad se hicieron con el arte de *Super Metroid*, que es de Nintendo, y aquí no se distribuye nada
suyo ([`../../NOTICE.es.md`](../../NOTICE.es.md)).

Los pones tú y pasas la carpeta con `--tiles`:

```powershell
python tools\package_uwp.py ... --tiles <tu carpeta>
```

En [`../../docs/ASSETS.es.md`](../../docs/ASSETS.es.md) están los trece nombres y las medidas
exactas. `package_uwp.py` los comprueba uno por uno, por nombre y por cabecera PNG, y se detiene con
la lista de los que falten en vez de dejarte un paquete que la consola enseña sin icono.

De los trece, solo dos se ven de verdad: `Square150x150Logo` (el mosaico de Mis juegos y
aplicaciones) y `SplashScreen` (la pantalla de carga).

El `.gitignore` bloquea `package/Assets/*.png`, así que puedes dejar tus mosaicos aquí mismo sin
riesgo: git no los va a coger y no puedes publicarlos por descuido.
