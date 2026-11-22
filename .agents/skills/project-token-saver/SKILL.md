---
name: project-token-saver
description: >-
  Guía de navegación rápida y ahorro de tokens para MCPE-1.0.0-Restored.
  Úsala siempre al buscar código, leer archivos o compilar en este proyecto
  para evitar greps/búsquedas masivas en build/, assets/ y MCPE-0.8.1/.
---

# Ahorro de Tokens y Búsqueda Rápida (`MCPE-1.0.0-Restored`)

Este repositorio contiene ~21,800 archivos, pero **el código activo es solo una pequeña fracción**. Buscar en la raíz (`.`) sin filtros desperdicia tiempo y tokens.

## 1. Mapa del Proyecto (Qué buscar y qué ignorar)

| Directorio | Contenido | Regla de búsqueda |
| :--- | :--- | :--- |
| `mcpe-engine/src/` | Código activo del motor (~778 archivos totales en `mcpe-engine/`) | **BUSCAR AQUÍ PRIMERO** |
| `handheld/src/`, `handheld/src-client/` | Código activo de juego y cliente | **BUSCAR AQUÍ** |
| `docs/` | Notas de investigación (UI, crafting, etc.) | Consultar si aplica |
| `handheld/src-deps/`, `mcpe-engine/src-deps/` | Dependencias de terceros | **EXCLUIR** salvo que se depure una lib |
| `MCPE-0.8.1/` | Código fuente de 0.8.1 para portar/extraer (~8,143 archivos) | **EXCLUIR** en búsquedas del código principal; buscar aquí **solo** al portar una clase/función de 0.8.1 |
| `assets/` | Texturas, sonidos, packs, mundos (~8,292 archivos) | **NO BUSCAR** salvo tareas de assets/JSON |
| `build/` | Artefactos CMake y `_deps` (~2,107 archivos) | **NUNCA BUSCAR AQUÍ** |
| `images/` | Capturas de pantalla | **NUNCA BUSCAR AQUÍ** |

## 2. Comandos de Búsqueda Óptimos

### Buscar solo en código activo (Recomendado)
Acota siempre la ruta a las carpetas activas:
```bash
rg -n "NombreSimbolo" mcpe-engine/src handheld/src handheld/src-client
```

### Si necesitas buscar desde la raíz
Usa exclusiones explícitas:
```bash
rg -n "NombreSimbolo" -g '!build/**' -g '!assets/**' -g '!MCPE-0.8.1/**' -g '!**/src-deps/**' -g '!images/**'
```

### Buscar archivos por nombre (sin `find .`)
```bash
rg --files mcpe-engine/src handheld/src handheld/src-client | rg -i "nombre"
```

### Reducir salida antes de leer
- Usa `rg -l "patron"` para listar solo rutas de archivos cuando esperas muchas coincidencias.
- Usa `rg -n --max-count 5 "patron" ...` para evitar inundar el contexto.

## 3. Lectura Económica de Archivos (Header-First)

1. **Headers primero**: en C++, lee primero el `.h` / `.hpp` para ver firmas, miembros y herencias antes de abrir el `.cpp`.
2. **Localiza antes de leer**: obtén el número de línea exacto con `rg -n`.
3. **Lee solo el bloque necesario**: usa `read_file` con `StartLine` y `EndLine` (±40-60 líneas alrededor de la función) en lugar de leer archivos `.cpp` completos.
4. **No re-leas** líneas que ya viste en la conversación si el archivo no ha cambiado.

## 4. Compilación Rápida y Control de Logs

El directorio `build/` ya está configurado con CMake (`Makefile` y `CMakeCache.txt` presentes). No busques archivos de build; compila directamente filtrando errores críticos:
```bash
cmake --build build -j$(nproc) 2>&1 | grep -E "error:|fatal error:" | head -n 25
```

## 5. Flujo por Subsistema y Símbolos (`clangd`)

- **Una sesión por subsistema**: al terminar un módulo (ej. renderizado, red, UI/crafting) y pasar a otro, usa `/compact` o abre una conversación nueva para no arrastrar contexto viejo.
- **`compile_commands.json` (opcional para `clangd` / MCP de símbolos)**: si configuras `clangd`, genera el índice en `build/` con:
```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```
