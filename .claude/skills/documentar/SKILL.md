---
name: documentar
description: Escribe o actualiza el reporte de un experimento del DASH Timing System con el formato de experimentos/exp0/experimento0_resultados.md, y actualiza docs/ESTADO.md. Úsala cuando el usuario diga "documenta", "haz el reporte", "escribe los resultados" o al cerrar un experimento o una prueba.
---

# Documentar un experimento

El reporte de referencia es `experimentos/exp0/experimento0_resultados.md`. Léelo antes de escribir y copia su tono: frases cortas, números siempre con unidad, cada afirmación respaldada por un dato o una gráfica.

## Antes de escribir

1. Reúne la evidencia: datos crudos en `experimentos/<id>/`, el sketch usado, el plan del experimento (p. ej. `docs/experimento0b_contexto.md`), la sección del Documento Maestro que lo define (la 16 para los experimentos) y la **predicción escrita en `docs/ESTADO.md`**.
2. Calcula los números desde los archivos crudos con un script (Python stdlib o el venv con matplotlib). No los copies de la conversación ni los redondees a mano.
3. Si faltan datos para una sección, escribe "pendiente" con lo que falta. No rellenes con estimaciones.

## Estructura (en este orden)

```markdown
# <Experimento N> — <nombre corto>

**Resultado:** <métrica principal con unidad> · <requisito que aplica y su límite> · **margen de <k>×** · Estado: **APROBADO | NO APROBADO | HALLAZGO**

---

## 1. Objetivo
Qué se cuantifica y por qué antes de seguir. La afirmación bajo prueba en cursiva.

## 2. Principio metrológico
Cuál es la referencia y por qué es más exacta que lo medido. Valor verdadero esperado.

## 3. Montaje
Tabla de conexiones (señal | hub | nodo o sensor), hardware, distancias, condiciones.

## 4. <Mecanismo o protocolo bajo prueba>
Fórmulas en $$…$$ y la justificación de cada parámetro con su número.

## 5. Procedimiento
Pasos numerados, como se ejecutaron de verdad. Al final: eventos registrados / esperados.

## 6. Resultados
Gráficas `![…](NN_nombre.png)` con una frase de lectura debajo de cada una.
### 6.1 Predicción contra resultado
| | Predicción (escrita el <fecha>) | Medido |
### 6.x Descomposición / tablas por escenario

## 7. Contexto en el presupuesto de error
Tabla fuente | estimado | medido, con la del Documento Maestro sección 6.

## 8. Conclusiones
Lista numerada. Cada conclusión en negritas + el número que la sostiene + qué ADR o requisito refuerza o pone en duda.

## 9. Incidencias resueltas durante la ejecución
| Síntoma | Causa raíz | Solución |
Incluye las hipótesis descartadas con el dato que las descartó.

## 10. Archivos
| Archivo | Contenido |
Todos los archivos de la carpeta del experimento, incluidos los intentos fallidos.
```

Se pueden omitir secciones que no apliquen (p. ej. la 4 en un barrido), pero no reordenar.

## Reglas

- **La predicción no se toca.** Se cita tal como está en `docs/ESTADO.md`, con su fecha, aunque haya fallado. Un resultado que la refuta es un hallazgo y se dice así.
- **Medido, provisional o estimado:** marca cada cifra nueva con su firmeza (tabla de firmeza de la evidencia del Documento Maestro, sección 0.2) cuando no sea obviamente medida.
- **Nomenclatura:** hub y nodo, no A y B (salvo al citar archivos históricos).
- **Gráficas:** si hacen falta, genera un script `graficar_<id>.py` en la carpeta del experimento que lea el crudo y escriba los PNG (`NN_nombre.png`). Usa la paleta de `experimentos/exp0/graficar.py` (TINTA `#1a1a1a`, ACENTO `#0f6f6a`, ALERTA `#c2410c`, SUAVE `#94a3b8`) para que todos los reportes se vean iguales.
- El reporte va en `experimentos/<id>/<id>_resultados.md` (o `experimento<N>_resultados.md`).
- **No edites el .docx del Documento Maestro.** Lo que cambie en él se agrega a la lista "Cambios pendientes para el Documento Maestro" de `docs/ESTADO.md`.

## Después de escribir

1. Actualiza `docs/ESTADO.md`: mueve la tarea a "Hecho" con la evidencia, ajusta "Ahora / Siguiente" y las decisiones abiertas.
2. Si cambió algo estable (un experimento aprobado, un ADR confirmado o en duda), actualiza la sección 2 de `CLAUDE.md`.
3. Propón el commit (mensaje en español: qué y por qué). No hagas push.
4. Responde con la línea de **Resultado**, la ruta del reporte y la lista de cambios propuestos al Documento Maestro.
