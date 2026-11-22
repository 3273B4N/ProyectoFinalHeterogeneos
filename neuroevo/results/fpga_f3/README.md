# Resultados de `v2_lanes8` (Kria KV260)

Config: kernel FP32 (sin punto fijo ni LUT), MNIST reducido, población 150, 1000 muestras, 100 generaciones, 5 corridas (semillas 42 a 46) → 500 muestras por etapa.

## Correctitud (FPGA vs CPU)

| Prueba | Dentro de tolerancia | Dif. máxima | Tolerancia |
|---|---|---|---|
| XOR armado a mano (pesos hasta ±30) | 8/8 | 0.0000 | 0.25 |
| MNIST, 40 genomas con ocultos (207 a 212 nodos, 200 muestras) | 40/40 | 0.0000 | 0.001 |

Coinciden porque ambos lados son FP32. No mide error de punto fijo ni de LUT.

## Perfilado por etapa (s por generación, 500 muestras)

| Etapa | Media | Desv. est. | Mín. | Mediana | p95 | Máx. | % |
|---|---|---|---|---|---|---|---|
| `t_kernel` | 5.6174 | 1.0061 | 4.0710 | 5.6234 | 7.0422 | 7.0630 | 99.6 |
| `t_d2h` | 0.0147 | 0.0007 | 0.0136 | 0.0146 | 0.0152 | 0.0209 | 0.3 |
| `t_pack` | 0.0078 | 0.0001 | 0.0075 | 0.0078 | 0.0079 | 0.0082 | 0.1 |
| `t_h2d` | 0.0018 | 0.0001 | 0.0017 | 0.0018 | 0.0019 | 0.0024 | 0.0 |
| `t_evaluar` | 5.6416 | 1.0062 | 4.0946 | 5.6476 | 7.0670 | 7.0873 | 100 |

- **El cuello de botella es el kernel.** `pack`, `h2d` y `d2h` suman menos del 0.5 %, y comparten DDR con la CPU, por eso mover datos cuesta casi nada.
- **La variabilidad viene solo del kernel** (desv. ≈ 1 s). Las demás etapas son estables.

## `t_kernel` durante la corrida (promedio de 10 generaciones)

| Corrida | Semilla | Gen. 0-9 | Gen. 90-99 | Máx. |
|---|---|---|---|---|
| 1 | 42 | 4.19 s | 5.69 s | 6.52 s |
| 2 | 43 | 4.27 s | 7.04 s | 7.04 s |
| 3 | 44 | 4.65 s | 7.03 s | 7.04 s |
| 4 | 45 | 4.43 s | 7.04 s | 7.06 s |
| 5 | 46 | 4.19 s | 6.77 s | 6.88 s |

- **El kernel sube de ~4.1 s a ~7.0 s y se estanca.** El promedio de conexiones casi no cambia (1970 → 1975), así que probablemente no es la causa.
- **Hipótesis (no medida):** el kernel recorre las conexiones una vez por capa y evalúa los genomas en secuencia, así que dependería de la profundidad de las redes y de los genomas más grandes, no del promedio.

## Tiempo total por corrida

| Corrida | Total | Evaluar | Especiar | Cruzar | Kernel (suma) | Train | Test |
|---|---|---|---|---|---|---|---|
| 1 | 1604.7 s | 32 % | 34 % | 34 % | 504.6 s | 62.9 % | 53.0 % |
| 2 | 1670.7 s | 35 % | 33 % | 33 % | 575.3 s | 64.6 % | 57.7 % |
| 3 | 1675.5 s | 35 % | 32 % | 33 % | 579.3 s | 62.3 % | 54.4 % |
| 4 | 1729.8 s | 37 % | 32 % | 32 % | 632.1 s | 65.8 % | 53.6 % |
| 5 | 1613.1 s | 32 % | 34 % | 34 % | 517.4 s | 58.9 % | 49.5 % |
| **Media** | **1658.8 s** | **34 %** | **33 %** | **33 %** | **561.7 s** | **62.9 %** | **53.6 %** |

- **La evaluación es solo ~34 % del tiempo total.** Especiación y cruce, en CPU, ocupan ~65 % (~5.5 s cada uno por generación, casi constantes). Acelerar solo el kernel tiene un techo.
- **Precisión:** contexto, no objetivo de F3. A 100 generaciones la red aún no converge.

## Límites del kernel

`MAX_NODES = 512` y `MAX_CONNS = 6144`. El host los valida y aborta con error si se pasan. Las 5 corridas terminaron sin ese error. No se registró el máximo, así que no se sabe qué tan cerca estuvieron.

## Limitaciones y siguientes pasos

- Sin punto fijo ni LUT de sigmoide (trabajo futuro).
- Validación con solo 40 genomas, ninguno cerca de los límites.
- Sin comparación de tiempo contra CPU.
- Siguiente paso: optimizar el kernel (paralelismo entre genomas, menos trabajo por capa) y mover o paralelizar especiación y cruce.

## Archivos

`validacion.txt`, `perfil_stats.csv`, `mnist_100gen_pop150_seed{42..46}_run{1..5}.csv`, `run{1..5}.log`.
