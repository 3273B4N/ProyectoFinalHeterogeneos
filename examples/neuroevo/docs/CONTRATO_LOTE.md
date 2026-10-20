# Contrato del lote (propuesta para acordar con Personas B y C)

Estado: **borrador de A**. Lo que está marcado como *(decidir)* hay que cerrarlo entre los tres.

## 1. Datos de entrada: `Batch` (ya implementado)

Formato plano, listo para subir tal cual a GPU/FPGA. Se sube **una sola vez** en `Evaluator::init`
(el conjunto de entrenamiento no cambia entre generaciones).

| Campo | Tipo | Forma | Notas |
|---|---|---|---|
| `inputs` | float32 | `[nSamples][nIn]`, fila-mayor | MNIST: 196 valores en [0,1]; XOR: 2 |
| `labels` | int32 | `[nSamples]` | clasificación (vacío si no aplica) |
| `targets` | float32 | `[nSamples][nOut]` | regresión (vacío si no aplica) |

Fitness: `Task::fitness(batch, outputs)` con `outputs` float32 `[nSamples][nOut]` fila-mayor.
Un evaluador de GPU/FPGA puede reproducir esa reducción en el dispositivo (MNIST: contar `argmax == label`,
`fitness = (aciertos/nSamples)^4`), pero la **referencia** es la función de la `Task`.

## 2. Población empaquetada *(decidir; A propone)*

La libreria guarda cada genoma como `std::vector<Node>` y `std::vector<Connection>`; no sirve para
dispositivo. Propuesta de formato CSR (estructura de arreglos), construido en el host cada generación:

```
nodeOff[popSize+1]   int32   // inicio de los nodos de cada genoma
connOff[popSize+1]   int32   // inicio de las conexiones de cada genoma
layer[totalNodes]    int32   // capa de cada nodo (0 = bias e inputs)
src[totalConns]      int32   // id LOCAL de nodo origen
dst[totalConns]      int32   // id LOCAL de nodo destino
w[totalConns]        float32 // peso
```

Reglas (salen de `Genome::runNetwork`, no son opcionales):

1. Ids locales: `0` = bias (valor fijo **1.0**), `1..nIn` = entradas, `nIn+1..nIn+nOut` = salidas, el resto ocultos.
2. Solo conexiones con `enabled == true`.
3. Orientar cada conexión hacia capa mayor (`layer[src] < layer[dst]`). Con `recurrent=false` ya viene así.
4. Orden de evaluación: para `l = 0 .. capaSalida-1`, acumular las conexiones cuyo origen está en la capa `l`
   y luego aplicar la activación a los nodos de la capa `l+1`.
5. Un nodo **no entrada sin conexiones entrantes** vale `activation(0)` (en sigmoide, 0.5), no 0. Es fácil olvidarlo.
6. Todas las salidas están en la misma (última) capa.
7. El bias en 1.0 es una corrección que hace `main` (`FIX_BIAS`); la librería lo deja en 0.

*(decidir)* ¿Quién escribe el empaquetador? Propuesta: A lo deja en `include/neuroevo/packing.hpp` para que GPU y FPGA
no tengan dos copias. *(decidir)* En FPGA, ¿tamaños máximos fijos (`maxNodes`, `maxConns` por genoma) con relleno?
Con `weightExtremumInit=1` y 1970 conexiones promedio en MNIST (CSV de perfilado) hay que fijar el tope.

## 3. Salida

Mínimo: `fitness[popSize]` float32, copiado a `genomes[i].fitness` dentro de `evaluate()`.
Opcional (depuración): `outputs[popSize][nSamples][nOut]`.

## 4. Tolerancia de validación *(decidir)*

Propuesta: con la misma semilla, la población evaluada en GPU/FPGA se compara con `CpuEvaluator` genoma a genoma.
- FP32: diferencia de fitness ≤ `1/nSamples` en ≥ 99 % de los genomas (empates de `argmax` y `exp` distinto pueden voltear una muestra).
- FP16 / punto fijo: reportar la diferencia como métrica del paper, no como fallo.
`tests/test_core.cpp` ya trae una red XOR armada a mano que sirve de caso exacto para cualquier evaluador.

## 5. Cronómetros

`evaluar` es el tiempo total del evaluador. Los sub-tiempos se declaran en `extraStages()` y salen como columnas
extra del CSV (`t_h2d`, `t_kernel`, `t_d2h`, ...). Nombres propuestos: `pack`, `h2d`, `kernel`, `d2h`.
Están **incluidos** en `evaluar`, no se suman aparte.
