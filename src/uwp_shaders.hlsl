// Triángulo de pantalla completa que muestrea la textura del cuadro. Sin vertex buffer.
struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

VSOut VSmain(uint id : SV_VertexID) {
  VSOut o;
  float2 t = float2((id << 1) & 2, id & 2);
  o.uv = t;
  o.pos = float4(t * float2(2, -2) + float2(-1, 1), 0, 1);
  return o;
}

Texture2D tex : register(t0);
SamplerState smp : register(s0);

float4 PSmain(float4 pos : SV_Position, float2 uv : TEXCOORD0) : SV_Target {
  return tex.Sample(smp, uv);
}
