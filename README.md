# Reporte de optimización

Antes de optimizar se perfiló el programa con dos herramientas independientes (`perf` y `gperftools`) bajo las siguientes condiciones: 200 generaciones, 2000 imágenes y una población de 150. Las dos coincidieron en que el tiempo se enfocaba en tres funciones:

- `Genome::runNetwork` (evaluar cada red contra las imágenes): **~59%**
  del tiempo total.
- `Population::crossover` (crear la siguiente generación de genomas):
  **~25%**.
- `Population::compareGenomes` (decidir a qué especie pertenece cada
  genoma): **~13%**.

Las otras funciones (carga de datos, mutación y función de activación) representaban menos del 2% del tiempo total. Con esto claro, los números mismos decidieron dónde se debía invertir el esfuerzo de paralelización: las dos primeras fases representan el 84 % del tiempo, por lo que son las que en realidad es importante optimizar.

## Optimizaciones realizadas

Se paralelizó `runNetwork`, ya que al ser la fase más costosa (59%), era la principal prioridad. Asimismo, resultó ser la más sencilla de las tres: cada genoma se analiza completamente por separado (no comparten información ni emplean aleatoriedad en esa función), por lo que dividir dicha evaluación entre múltiples hilos con `#pragma omp parallel for` no suponía ningún peligro de condición de carrera. Sin embargo, para evitar que varios hilos se sobreescribieran al escribir en ella, el único cambio requerido fue que cada genoma tuviera su propia copia de los datos auxiliares que antes eran compartidos por todos (una sola variable global). 

 Por otra parte, se paralelizó `crossover, ya que se volvió el siguiente objetivo lógico y, sin ser modificado, se transformó en el nuevo cuello de botella. La función utilizaba `rand()`, que emplea un solo generador para todo el programa (no es seguro si varios hilos lo llaman a la vez); además, elaboraba genomas que, aunque iban a ser eliminados inmediatamente, accedían a una tabla de identificadores de conexión compartida y almacenaban cada nuevo hijo en el mismo vector con `push_back`, lo cual tampoco es seguro entre hilos. Cada uno de los tres problemas fue resuelto individualmente (utilizando una semilla aleatoria propia por hilo, impidiendo que los genomas desechables creen conexiones innecesarias y protegiendo únicamente el `push_back` con una sección crítica), lo cual hizo posible paralelizar `crossover` de manera segura. Esto mantuvo la lógica del algoritmo, aunque se perdió la reproducibilidad precisa con la misma semilla al azar (ya no es el mismo el orden en que se generan los números aleatorios).

## Por qué no se aplicaron otras optimizaciones

- **Ordenamiento o alineación de las estructuras de datos**: se descartó debido a la misma razón prioritaria, además respaldada por los propios contadores de hardware del perfilado. La tasa de fallos de caché medida era muy baja, lo que indicaba que el acceso a la memoria no era el problema. Si el perfilado hubiera revelado una cantidad significativa de errores de caché, habría sido razonable optimizarlo; no obstante, como eso no sucedió, no valía la pena invertir esfuerzo en él en lugar del paralelismo, que sí presentaba pruebas contundentes de ser el verdadero cuello de botella.
- **Opciones de compilador**: no se implementaron, ya que el perfilado ya había indicado claramente que el paralelismo era la palanca principal (84% del tiempo en dos funciones paralelizables). Estas opciones conllevan riesgos que no justifican ser asumidos para una ganancia menor, ya que la compilación específica para una CPU podría afectar la portabilidad entre las computadoras y el sistema empotrado.

## Resultado 

Con las dos optimizaciones aplicadas, el tiempo total (200 gen / 2000 img / pob 150, semilla 42, medido 5 veces con `perf stat`) bajó de **193.95 s ± 1.06 s** a **90.91 s ± 5.10 s**, una mejora de **~2.13×**.
