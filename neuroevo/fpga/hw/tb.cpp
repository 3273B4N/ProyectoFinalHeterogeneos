// testbench: compara el kernel contra una referencia en double (misma regla que CpuEvaluator)
#include <cstdio>
#include <cmath>
#include <vector>
#include <random>
#include <algorithm>
#include "neat_eval_hls.h"

struct Red {
  std::vector<int> capa;
  std::vector<int> src, dst;
  std::vector<float> w;
};

// lote en CSR, igual que PackedBatch
struct Lote {
  std::vector<int32_t> nodeOff{0}, connOff{0}, layer, src, dst;
  std::vector<data_t> w;
};

static void agregar(Lote& L, const Red& r) {
  int base = L.nodeOff.back();
  for (int c : r.capa) L.layer.push_back(c);
  for (size_t i = 0; i < r.src.size(); i++) {
    L.src.push_back(base + r.src[i]);
    L.dst.push_back(base + r.dst[i]);
    L.w.push_back(r.w[i]);
  }
  L.nodeOff.push_back(base + (int)r.capa.size());
  L.connOff.push_back(L.connOff.back() + (int)r.src.size());
}

template <class T> static void pad(std::vector<T>& v, size_t n) { if (v.size() < n) v.resize(n); }
static void llamar(Lote& L, std::vector<data_t>& in, std::vector<data_t>& out,
                   int nG, int nS, int nIn, int nOut) {
  pad(L.nodeOff, 16); pad(L.connOff, 16); pad(L.layer, 1024);
  pad(L.src, 8192); pad(L.dst, 8192); pad(L.w, 8192);
  pad(in, 8192); pad(out, 8192);
  neat_eval_hls(L.nodeOff.data(), L.connOff.data(), L.layer.data(), L.src.data(), L.dst.data(),
                L.w.data(), in.data(), out.data(), nG, nS, nIn, nOut);
}

static void referencia(const Red& r, const float* img, int nIn, int nOut, double* out) {
  int n = (int)r.capa.size();
  std::vector<double> val(n, 0.0);
  val[0] = 1.0;
  for (int i = 1; i <= nIn; i++) val[i] = img[i - 1];
  int capaMax = *std::max_element(r.capa.begin(), r.capa.end());
  for (int l = 1; l <= capaMax; l++)
    for (int v = nIn + 1; v < n; v++) {
      if (r.capa[v] != l) continue;
      double s = 0.0;
      for (size_t c = 0; c < r.src.size(); c++)
        if (r.dst[c] == v) s += (double)r.w[c] * val[r.src[c]];
      val[v] = 1.0 / (1.0 + std::exp(-s));
    }
  for (int k = 0; k < nOut; k++) out[k] = val[nIn + 1 + k];
}

// XOR armado a mano, igual que en tests/test_core.cpp
static Red redXor() {
  Red r;
  r.capa = {0, 0, 0, 2, 1, 1};  // bias, x1, x2, salida, oculto OR, oculto AND
  auto c = [&](int a, int b, float w) { r.src.push_back(a); r.dst.push_back(b); r.w.push_back(w); };
  c(0, 4, -10); c(1, 4, 20); c(2, 4, 20);
  c(0, 5, -30); c(1, 5, 20); c(2, 5, 20);
  c(0, 3, -10); c(4, 3, 20); c(5, 3, -20);
  return r;
}

// red aleatoria tipo NEAT; completa = todas las entradas conectadas a las salidas (como MNIST)
static Red redAleatoria(std::mt19937& rng, int nIn, int nOut, int nOcultos, int capas, bool completa) {
  Red r;
  std::uniform_real_distribution<float> peso(-1.0f, 1.0f);
  int capaSal = capas;
  r.capa.assign(1 + nIn, 0);
  for (int k = 0; k < nOut; k++) r.capa.push_back(capaSal);
  for (int h = 0; h < nOcultos; h++) r.capa.push_back(1 + (int)(rng() % (capaSal - 1 > 0 ? capaSal - 1 : 1)));
  int n = (int)r.capa.size();
  auto c = [&](int a, int b) { r.src.push_back(a); r.dst.push_back(b); r.w.push_back(peso(rng)); };
  if (completa)
    for (int a = 0; a <= nIn; a++)
      for (int k = 0; k < nOut; k++) c(a, nIn + 1 + k);
  int extra = completa ? nOcultos * 3 : n * 3;
  for (int e = 0; e < extra; e++) {
    int a = rng() % n, b = rng() % n;
    if (r.capa[a] < r.capa[b]) c(a, b);
  }
  std::vector<size_t> idx(r.src.size());
  for (size_t i = 0; i < idx.size(); i++) idx[i] = i;
  std::shuffle(idx.begin(), idx.end(), rng);
  Red m = r;
  for (size_t i = 0; i < idx.size(); i++) { m.src[i] = r.src[idx[i]]; m.dst[i] = r.dst[idx[i]]; m.w[i] = r.w[idx[i]]; }
  return m;
}

static int probar(const char* nombre, const std::vector<Red>& redes, const std::vector<float>& imgs,
                  int nSamples, int nIn, int nOut, double tol) {
  Lote L;
  for (const Red& r : redes) agregar(L, r);
  int nG = (int)redes.size();
  std::vector<data_t> in(imgs.begin(), imgs.end());
  std::vector<data_t> out((size_t)nG * nSamples * nOut, (data_t)-7);
  
  int conexiones = L.connOff.back();   // antes de llamar(): el padding cambia back()
  llamar(L, in, out, nG, nSamples, nIn, nOut);

  double err = 0.0;

  std::vector<double> ref(nOut);
  for (int g = 0; g < nG; g++)
    for (int m = 0; m < nSamples; m++) {
      referencia(redes[g], &imgs[(size_t)m * nIn], nIn, nOut, ref.data());
      for (int k = 0; k < nOut; k++) {
        double d = std::fabs((double)out[((size_t)g * nSamples + m) * nOut + k] - ref[k]);
        if (d > err) err = d;
      }
    }
  printf("%-28s genomas=%-3d imagenes=%-4d conexiones=%-6d error max=%.3e  %s\n",
         nombre, nG, nSamples, conexiones, err, err <= tol ? "OK" : "FALLO");
  return err <= tol ? 0 : 1;
}

int main() {
  const double tol = 1e-4;
  int fallos = 0;
  std::mt19937 rng(42);
  std::uniform_real_distribution<float> pix(0.0f, 1.0f);

  // 1) XOR: las 4 combinaciones de entrada
  {
    std::vector<float> imgs = {0, 0, 0, 1, 1, 0, 1, 1};
    fallos += probar("XOR armado a mano", {redXor()}, imgs, 4, 2, 1, tol);
    Lote L; agregar(L, redXor());
    std::vector<data_t> in(imgs.begin(), imgs.end()), out(4);
    llamar(L, in, out, 1, 4, 2, 1);
    int esperado[4] = {0, 1, 1, 0}, ok = 1;
    for (int i = 0; i < 4; i++) ok &= ((out[i] > 0.5f) == (esperado[i] == 1));
    printf("%-28s salidas = %.4f %.4f %.4f %.4f  %s\n", "XOR, tabla de verdad",
           (float)out[0], (float)out[1], (float)out[2], (float)out[3], ok ? "OK" : "FALLO");
    fallos += ok ? 0 : 1;
  }

  // 2) redes pequenas aleatorias, 13 imagenes (no multiplo de los carriles)
  {
    int nIn = 5, nOut = 3, nS = 13;
    std::vector<Red> redes;
    for (int g = 0; g < 8; g++) redes.push_back(redAleatoria(rng, nIn, nOut, 2 + g, 2 + g % 4, false));
    std::vector<float> imgs(nS * nIn);
    for (auto& v : imgs) v = pix(rng);
    fallos += probar("redes aleatorias pequenas", redes, imgs, nS, nIn, nOut, tol);
  }

  // 3) tamano MNIST: 196 -> 10
  {
    int nIn = 196, nOut = 10, nS = 37;
    std::vector<Red> redes;
    for (int g = 0; g < 3; g++) redes.push_back(redAleatoria(rng, nIn, nOut, 5 * g, 1 + g, true));
    std::vector<float> imgs(nS * nIn);
    for (auto& v : imgs) v = pix(rng);
    fallos += probar("tamano MNIST (196 -> 10)", redes, imgs, nS, nIn, nOut, tol);
  }

  printf(fallos ? "RESULTADO: FALLO\n" : "RESULTADO: OK\n");
  return fallos ? 1 : 0;
}
