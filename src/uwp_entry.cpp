// uwp_entry.cpp — punto de entrada del port UWP (Xbox Series X, modo desarrollador) de
// SuperMetroidRecomp.
//
// Este juego NO comparte el molde de los tres Donkey Kong Country: su host es un `main.c`
// de ~2 300 líneas construido sobre SDL, así que aquí no se reescribe el host, se le pone
// alrededor lo que UWP exige y se deja al juego hacer lo suyo.
//
//  - En UWP el proceso no arranca por `main()` sino por CoreApplication. El SDL3 para UWP
//    (el mismo que usa el port de Mario Kart Wii en esta consola) trae ese cascarón dentro
//    de `SDL_RunApp`, que crea la CoreWindow y llama a la función de la app en su hilo.
//  - El juego dibuja con el presentador por SDL_Renderer (respaldado por Direct3D 11), que
//    es el que usa en Android. La rama de OpenGL se compila fuera: la Xbox no la permite.
//  - El directorio de instalación es de SOLO LECTURA. El juego hace toda su entrada/salida
//    con rutas relativas (`config.ini`, `rom.cfg`, `saves\`, `sm-video.ini`), así que antes
//    de entrar se cambia el directorio de trabajo a la carpeta de datos de la app y se
//    siembran ahí los dos archivos que hacen falta para arrancar sin lanzador.

#include <windows.h>

#include <SDL3/SDL.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "uwp_shaders.h"  // g_VS[]/g_VS_len y g_PS[]/g_PS_len (precompilados con fxc)

extern "C" {
#include "util.h"  // struct RendererFuncs (el conector de presentadores del motor)
}

// main() del juego, renombrado por el parche SM_UWP de src/main.c.
extern "C" int SmRecompMain(int argc, char **argv);

// Traza de arranque: en la consola no hay consola ni depurador al que escribir. Arranca en
// la carpeta temporal del paquete y, en cuanto se conoce la carpeta de datos de la app, se
// muda ahí, que es la que el Device Portal deja leer sin vueltas (\LocalState\...).
static std::wstring g_traza;

// Serializada: desde que hay hilo de presentación escriben aquí dos hilos, y dos aperturas
// a la vez se pisan (una falla y su línea se pierde, incluidos los latidos que se usan para
// medir la cadencia).
static std::mutex g_traza_mtx;

static void Traza(const char *texto) {
  std::lock_guard<std::mutex> cerrojo(g_traza_mtx);
  if (g_traza.empty()) {
    wchar_t tmp[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmp);
    g_traza = std::wstring(tmp) + L"sm-uwp-traza.txt";
  }
  HANDLE h = CreateFile2(g_traza.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                         OPEN_ALWAYS, nullptr);
  if (h == INVALID_HANDLE_VALUE) return;
  const std::string linea = std::string(texto) + "\r\n";
  DWORD escrito = 0;
  WriteFile(h, linea.data(), static_cast<DWORD>(linea.size()), &escrito, nullptr);
  CloseHandle(h);
}

// Puente para el motor: host_report_breadcrumb (parcheado bajo SM_UWP) manda aquí cada miga
// de pan del juego, que si no se perdería (en la consola no hay consola ni depurador).
extern "C" void SmUwpTraza(const char *texto) { Traza(texto); }

static void TrazaEnCarpetaDeDatos(const std::string &datos) {
  const std::string ruta = datos + "sm-uwp-traza.txt";
  g_traza.assign(ruta.begin(), ruta.end());
  Traza("traza mudada a la carpeta de datos");
}

// Carpeta del ejecutable = raíz del paquete instalado (solo lectura).
static std::string DirectorioDelPaquete() {
  char buf[MAX_PATH]{};
  const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  std::string ruta(buf, n);
  const size_t corte = ruta.find_last_of('\\');
  return corte == std::string::npos ? std::string(".") : ruta.substr(0, corte);
}

static bool Existe(const std::string &ruta) {
  FILE *f = fopen(ruta.c_str(), "rb");
  if (!f) return false;
  fclose(f);
  return true;
}

static void Escribe(const std::string &ruta, const std::string &contenido) {
  FILE *f = fopen(ruta.c_str(), "wb");
  if (!f) return;
  fwrite(contenido.data(), 1, contenido.size(), f);
  fclose(f);
}

// Para lo que el usuario puede cambiar dentro del juego: se siembra una vez y no se pisa.
static void EscribeSiFalta(const std::string &ruta, const std::string &contenido) {
  if (Existe(ruta)) return;
  Escribe(ruta, contenido);
}

// Deja el directorio de trabajo en la carpeta de datos de la app y siembra la configuración
// mínima: sin lanzador (en la consola no hay ratón para él) y con la ROM del paquete.
static std::string g_config_ini;
static std::string g_rom;

static bool PreparaCarpetaDeDatos(const std::string &paquete) {
  char *pref = SDL_GetPrefPath(nullptr, "SuperMetroidRecomp");
  if (!pref) {
    Traza("SDL_GetPrefPath devolvio null");
    return false;
  }
  const std::string datos(pref);
  SDL_free(pref);
  if (!SetCurrentDirectoryA(datos.c_str())) {
    Traza(("SetCurrentDirectory fallo en " + datos).c_str());
    return false;
  }
  TrazaEnCarpetaDeDatos(datos);
  Traza(("datos: " + datos).c_str());

  const std::string rom = paquete + "\\sm.sfc";
  Traza(("rom: " + rom + (Existe(rom) ? "  (existe)" : "  (NO EXISTE)")).c_str());
  // rom.cfg SIEMPRE se reescribe: la ruta del paquete lleva el número de versión dentro, así
  // que al reinstalar una versión nueva la cacheada deja de existir y el juego se iría al
  // selector de ROM, que en la consola no existe.
  Escribe("rom.cfg", rom + "\n");

  // OutputMethod = SDL fuerza el presentador por SDL_Renderer (Direct3D 11 en la consola).
  // SkipLauncher = 1 evita la ventana del lanzador, que aquí no se puede manejar.
  EscribeSiFalta("config.ini",
                 "[General]\r\nSkipLauncher = 1\r\n\r\n"
                 "[Graphics]\r\nOutputMethod = SDL\r\nFullscreen = 1\r\nLinearFiltering = 1\r\n\r\n"
                 "[Sound]\r\nEnableAudio = 1\r\nAudioFreq = 48000\r\n");

  // El 16:9 de este juego no es un mod del núcleo como en los DKC: es un ajuste del
  // renderizador propio, que en PC escribe el lanzador. Aquí se siembra a mano.
  // PresentThread no es del juego: lo lee el presentador de este archivo (ver el bloque del
  // hilo de presentación). Con 0 vuelve al camino síncrono, que es el A/B de la medición.
  EscribeSiFalta("sm-video.ini",
                 "[SuperMetroidVideo]\r\nEnhancedRenderer=1\r\nAspect=16:9\r\n"
                 "PresentationEnabled=0\r\nPresentationFPS=0\r\nHudAnchored=1\r\n"
                 "PresentThread=1\r\n");

  // Todo lo que el juego y el motor cuentan por stderr/stdout se pierde en la consola.
  // Redirigidos a archivo, se bajan con el Device Portal igual que la traza.
  freopen((datos + "sm-stderr.txt").c_str(), "w", stderr);
  freopen((datos + "sm-stdout.txt").c_str(), "w", stdout);
  setvbuf(stderr, nullptr, _IONBF, 0);
  setvbuf(stdout, nullptr, _IONBF, 0);

  g_config_ini = datos + "config.ini";
  g_rom = rom;
  return true;
}

// ───────────────────────── Presentador Direct3D 11 ──────────────────────────
//
// El presentador por SDL_Renderer NO sirve en esta consola: su backend de Direct3D 11 pide
// el HWND de la ventana (`SDL_CreateRenderer` falla con «Couldn't get window handle») y el
// backend de vídeo de SDL en UWP es WinRT, donde no hay HWND sino una CoreWindow. Por eso
// aquí se implementa el conector `RendererFuncs` del propio motor con Direct3D 11 sobre la
// CoreWindow, que es exactamente lo que hacen los tres Donkey Kong Country.
//
// El juego entrega el cuadro terminado en un búfer de CPU (BGRA, ancho×alto): BeginDraw le
// presta ese búfer y EndDraw lo sube a una textura y lo dibuja a pantalla completa.

using Microsoft::WRL::ComPtr;

namespace {

ComPtr<ID3D11Device> g_dev;
ComPtr<ID3D11DeviceContext> g_ctx;
ComPtr<IDXGISwapChain1> g_swap;
ComPtr<ID3D11RenderTargetView> g_rtv;
ComPtr<ID3D11Texture2D> g_tex;
ComPtr<ID3D11ShaderResourceView> g_srv;
ComPtr<ID3D11SamplerState> g_samp;
ComPtr<ID3D11VertexShader> g_vs;
ComPtr<ID3D11PixelShader> g_ps;
UINT g_bbw = 1920, g_bbh = 1080;
int g_fw = 0, g_fh = 0;             // tamaño del cuadro que pide el juego
std::vector<uint8_t> g_frame;       // búfer de CPU que se le presta

// ─────────────────────────── Hilo de presentación ───────────────────────────
//
// El bucle del juego hacía todo en un hilo: emular, capturar, componer y presentar. El
// Present con espera al vertical costaba ~4,4 ms de los 16,7 que dura un cuadro, y con eso
// la SIMULACIÓN se quedaba en 52-55 Hz aunque la presentación aguantara 58-59 (medido en la
// consola el 2026-09-11). Peor: al descolgarse, el reloj del juego dispara presentaciones
// de más, y cada una vuelve a cobrar componer + presentar.
//
// Aquí EndDraw ya no dibuja: copia el cuadro terminado a un búfer de entrega y despierta a
// este hilo, que es **el único que toca el contexto de Direct3D 11** y el único que espera
// al vertical. El hilo del juego no se bloquea nunca; si va más rápido que la pantalla, el
// cuadro entregado se pisa (se cuenta en `g_soltados`) y no pasa nada.
//
// Se puede apagar sin recompilar: `PresentThread=0` en `sm-video.ini` vuelve al camino
// síncrono de antes, que es como se hizo el A/B.
bool g_con_hilo = true;
std::thread g_hilo;
std::mutex g_mtx;
std::condition_variable g_cv;
std::vector<uint8_t> g_entrega;     // cuadro terminado esperando turno
std::vector<uint8_t> g_subida;      // el que el hilo está subiendo ahora
bool g_hay_cuadro = false;
bool g_parar = false;
unsigned long long g_presentados = 0, g_soltados = 0;

void HiloDePresentacion();
void DibujaYPresenta(const uint8_t *pixeles);

// Por defecto encendido: si el juego reescribe sm-video.ini (su escritor solo conoce sus
// cinco claves) la línea se pierde, y entonces vale el valor bueno.
bool LeeAjusteDelHilo() {
  FILE *f = fopen("sm-video.ini", "r");
  if (!f) return true;
  char linea[256], clave[64], valor[64];
  bool con_hilo = true;
  while (fgets(linea, sizeof linea, f)) {
    if (sscanf(linea, " %63[^= \t] = %63s", clave, valor) != 2) continue;
    if (std::strcmp(clave, "PresentThread") == 0) con_hilo = valor[0] != '0';
  }
  fclose(f);
  return con_hilo;
}

void CreaRtv() {
  ComPtr<ID3D11Texture2D> bb;
  g_swap->GetBuffer(0, __uuidof(ID3D11Texture2D), &bb);
  g_dev->CreateRenderTargetView(bb.Get(), nullptr, g_rtv.ReleaseAndGetAddressOf());
}

// La llama el hilo del juego. La textura y su vista las lee el hilo de presentación, así
// que se crean aparte y se publican bajo el cerrojo; el cuadro entregado que hubiera se
// descarta, porque ya no cuadra con el tamaño nuevo.
void AseguraTextura(int w, int h) {
  if (g_tex && w == g_fw && h == g_fh) return;
  D3D11_TEXTURE2D_DESC td = {};
  td.Width = static_cast<UINT>(w);
  td.Height = static_cast<UINT>(h);
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DYNAMIC;
  td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
  ComPtr<ID3D11Texture2D> tex;
  ComPtr<ID3D11ShaderResourceView> srv;
  if (FAILED(g_dev->CreateTexture2D(&td, nullptr, tex.GetAddressOf()))) return;
  if (FAILED(g_dev->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf()))) return;

  std::lock_guard<std::mutex> cerrojo(g_mtx);
  g_fw = w;
  g_fh = h;
  g_frame.assign(static_cast<size_t>(w) * h * 4, 0);
  g_entrega.assign(g_frame.size(), 0);
  g_hay_cuadro = false;
  g_tex = tex;
  g_srv = srv;
}

bool Inicializa(SDL_Window *window) {
  // La CoreWindow sale de las propiedades de la ventana de SDL: en UWP es lo que hay en
  // lugar del HWND, y es lo que pide CreateSwapChainForCoreWindow.
  IUnknown *coreWindow = static_cast<IUnknown *>(SDL_GetPointerProperty(
      SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WINRT_WINDOW_POINTER, nullptr));
  if (!coreWindow) {
    Traza("presentador: la ventana de SDL no expone la CoreWindow");
    return false;
  }
  int w = 0, h = 0;
  SDL_GetWindowSizeInPixels(window, &w, &h);
  g_bbw = w > 0 ? static_cast<UINT>(w) : 1920;
  g_bbh = h > 0 ? static_cast<UINT>(h) : 1080;

  D3D_FEATURE_LEVEL fl[] = {D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0};
  HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
                                 D3D11_CREATE_DEVICE_BGRA_SUPPORT, fl, _countof(fl),
                                 D3D11_SDK_VERSION, g_dev.GetAddressOf(), nullptr,
                                 g_ctx.GetAddressOf());
  if (FAILED(hr)) {
    Traza("presentador: D3D11CreateDevice fallo");
    return false;
  }
  ComPtr<IDXGIDevice1> dxgiDev;
  g_dev.As(&dxgiDev);
  ComPtr<IDXGIAdapter> adapter;
  dxgiDev->GetAdapter(adapter.GetAddressOf());
  ComPtr<IDXGIFactory2> factory;
  adapter->GetParent(__uuidof(IDXGIFactory2), &factory);

  DXGI_SWAP_CHAIN_DESC1 sd = {};
  sd.Width = g_bbw;
  sd.Height = g_bbh;
  sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  sd.SampleDesc.Count = 1;
  sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount = 2;
  sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  sd.Scaling = DXGI_SCALING_STRETCH;
  hr = factory->CreateSwapChainForCoreWindow(g_dev.Get(), coreWindow, &sd, nullptr,
                                             g_swap.GetAddressOf());
  if (FAILED(hr)) {
    Traza("presentador: CreateSwapChainForCoreWindow fallo");
    return false;
  }
  CreaRtv();

  D3D11_SAMPLER_DESC smp = {};
  smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
  smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
  g_dev->CreateSamplerState(&smp, g_samp.GetAddressOf());
  g_dev->CreateVertexShader(g_VS, g_VS_len, nullptr, g_vs.GetAddressOf());
  g_dev->CreatePixelShader(g_PS, g_PS_len, nullptr, g_ps.GetAddressOf());

  g_con_hilo = LeeAjusteDelHilo();
  if (g_con_hilo) {
    g_parar = false;
    g_hilo = std::thread(HiloDePresentacion);
  }

  char msg[128];
  snprintf(msg, sizeof msg, "presentador D3D11 listo: %ux%u  hilo de presentacion=%s",
           g_bbw, g_bbh, g_con_hilo ? "si" : "no");
  Traza(msg);
  return true;
}

void Destruye() {
  if (g_hilo.joinable()) {
    {
      std::lock_guard<std::mutex> cerrojo(g_mtx);
      g_parar = true;
    }
    g_cv.notify_all();
    g_hilo.join();
  }
  char msg[128];
  snprintf(msg, sizeof msg, "presentador: total %llu presentados, %llu soltados",
           g_presentados, g_soltados);
  Traza(msg);
  g_srv.Reset();
  g_tex.Reset();
  g_rtv.Reset();
  g_swap.Reset();
  g_ctx.Reset();
  g_dev.Reset();
}

void TamanoDeSalida(int *width, int *height) {
  *width = static_cast<int>(g_bbw);
  *height = static_cast<int>(g_bbh);
}

void EmpiezaDibujo(int width, int height, uint8 **pixels, int *pitch) {
  AseguraTextura(width, height);
  *pixels = g_frame.data();
  *pitch = width * 4;
}

// Sube el cuadro a la textura, lo dibuja y **espera al vertical** (Present con intervalo 1).
// Es el único sitio que usa el contexto de Direct3D 11, y lo ejecuta un solo hilo: el de
// presentación, o el del juego si el hilo está apagado.
void DibujaYPresenta(const uint8_t *pixeles) {
  ComPtr<ID3D11Texture2D> tex;
  ComPtr<ID3D11ShaderResourceView> srv;
  int fw = 0, fh = 0;
  {
    std::lock_guard<std::mutex> cerrojo(g_mtx);
    tex = g_tex;
    srv = g_srv;
    fw = g_fw;
    fh = g_fh;
  }
  if (!tex || !g_swap || !pixeles || fw <= 0 || fh <= 0) return;

  D3D11_MAPPED_SUBRESOURCE m;
  if (SUCCEEDED(g_ctx->Map(tex.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
    const size_t fila = static_cast<size_t>(fw) * 4;
    for (int y = 0; y < fh; ++y)
      std::memcpy(static_cast<uint8_t *>(m.pData) + static_cast<size_t>(y) * m.RowPitch,
                  pixeles + static_cast<size_t>(y) * fila, fila);
    g_ctx->Unmap(tex.Get(), 0);
  }
  D3D11_VIEWPORT vp = {0, 0, static_cast<float>(g_bbw), static_cast<float>(g_bbh), 0, 1};
  g_ctx->RSSetViewports(1, &vp);
  ID3D11RenderTargetView *rtv = g_rtv.Get();
  g_ctx->OMSetRenderTargets(1, &rtv, nullptr);
  g_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
  g_ctx->IASetInputLayout(nullptr);
  g_ctx->VSSetShader(g_vs.Get(), nullptr, 0);
  g_ctx->PSSetShader(g_ps.Get(), nullptr, 0);
  ID3D11ShaderResourceView *vista = srv.Get();
  g_ctx->PSSetShaderResources(0, 1, &vista);
  ID3D11SamplerState *smp = g_samp.Get();
  g_ctx->PSSetSamplers(0, 1, &smp);
  g_ctx->Draw(3, 0);
  g_swap->Present(1, 0);

  if ((++g_presentados % 600) == 0) {
    char msg[128];
    snprintf(msg, sizeof msg, "presentador: %llu presentados, %llu soltados",
             g_presentados, g_soltados);
    Traza(msg);
  }
}

void HiloDePresentacion() {
  Traza("presentador: hilo arrancado");
  for (;;) {
    {
      std::unique_lock<std::mutex> cerrojo(g_mtx);
      g_cv.wait(cerrojo, [] { return g_hay_cuadro || g_parar; });
      if (!g_hay_cuadro) break;               // solo queda la orden de parar
      g_subida.swap(g_entrega);               // intercambio de punteros, no copia
      g_hay_cuadro = false;
    }
    DibujaYPresenta(g_subida.data());
  }
  Traza("presentador: hilo terminado");
}

// Lo llama el juego al terminar de componer el cuadro. Con el hilo encendido esto NO
// dibuja: copia el cuadro al búfer de entrega (unos 300 KB, del orden de 0,05 ms) y sigue.
// El hilo del juego no espera nunca al vertical.
void TerminaDibujo() {
  if (!g_con_hilo) {
    DibujaYPresenta(g_frame.data());
    return;
  }
  {
    std::lock_guard<std::mutex> cerrojo(g_mtx);
    if (g_frame.empty()) return;
    if (g_hay_cuadro) ++g_soltados;           // el hilo no alcanzó a subir el anterior
    if (g_entrega.size() != g_frame.size()) g_entrega.resize(g_frame.size());
    std::memcpy(g_entrega.data(), g_frame.data(), g_frame.size());
    g_hay_cuadro = true;
  }
  g_cv.notify_one();
}

}  // namespace

// Lo llama src/main.c (bajo SM_UWP) en lugar de elegir entre OpenGL y SDL_Renderer.
extern "C" void SmUwpRenderer_Create(struct RendererFuncs *funcs) {
  funcs->Initialize = Inicializa;
  funcs->Destroy = Destruye;
  funcs->GetOutputSize = TamanoDeSalida;
  funcs->BeginDraw = EmpiezaDibujo;
  funcs->EndDraw = TerminaDibujo;
}

static int Arranque(int argc, char **argv) {
  Traza("bootstrap: dentro de SDL_RunApp");
  const std::string paquete = DirectorioDelPaquete();
  Traza(("paquete: " + paquete).c_str());
  PreparaCarpetaDeDatos(paquete);

  // Mandos de la consola por Windows.Gaming.Input (SDL3-uwp lo trae opcional).
  SDL_SetHint(SDL_HINT_JOYSTICK_WGI, "true");

  // El renderizador se fija a Direct3D 11 en vez de dejar que SDL recorra su lista: en la
  // consola el resto de los backends (d3d9, opengl, vulkan) o no existen o no están
  // permitidos, y la app se caía dentro de SDL_CreateRenderer al tantearlos.
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d11");

  // Sin argumentos, el juego reancla el directorio de trabajo al del ejecutable
  // (snesrecomp_anchor_to_exe_dir), que aquí es el paquete instalado y es de SOLO LECTURA:
  // entonces no encontraría ni config.ini ni rom.cfg y se iría al selector de ROM. Su propia
  // opción `--config <ruta>` salta ese reanclaje, así que el directorio de trabajo sigue
  // siendo la carpeta de datos y todo lo relativo (rom.cfg, saves\, keybinds.ini) cae donde
  // sí se puede escribir.
  //
  // Y la ROM va como argumento posicional: el atajo que lee `rom.cfg` vive dentro del
  // bloque del lanzador GUI (`#if defined(SNES_LAUNCHER) || defined(RECOMP_LAUNCHER)`), que
  // aquí no se compila. Sin argumento, el juego cae en el selector de archivos y se rinde.
  // Con la ruta delante, el resolvedor la verifica por SHA-256 y arranca.
  char arg0[] = "SuperMetroidRecomp";
  char arg1[] = "--config";
  std::string cfg = g_config_ini.empty() ? std::string("config.ini") : g_config_ini;
  std::vector<char> arg2(cfg.begin(), cfg.end());
  arg2.push_back('\0');
  std::vector<char> arg3(g_rom.begin(), g_rom.end());
  arg3.push_back('\0');
  char *args[] = {arg0, arg1, arg2.data(), arg3.data(), nullptr};
  (void)argc;
  (void)argv;

  // El perfilador por etapas del juego se enciende por variable de entorno; aqui se
  // deja siempre puesto, porque en la consola es la unica forma de ver donde se va el
  // tiempo (el informe sale en el latido, parche SM_UWP de main.c).
  _putenv_s("SM_PROFILE", "1");
  _putenv_s("SM_RTL_DIAG", "1");

  Traza(("entrando a SmRecompMain: --config " + cfg + "  rom " + g_rom).c_str());
  const int rc = SmRecompMain(4, args);
  Traza(("SmRecompMain devolvio " + std::to_string(rc)).c_str());
  return rc;
}

int CALLBACK WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
  Traza("WinMain: proceso vivo");
  return SDL_RunApp(0, nullptr, Arranque, nullptr);
}
