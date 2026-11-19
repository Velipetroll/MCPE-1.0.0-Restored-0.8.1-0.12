# Auditoría de UI/GUI: Crafteo e Inventario MCPE 0.8.1 Touch vs MCPE 1.0.0-Restored

Este documento compara línea por línea la arquitectura real de inventario, crafteo y armadura de **MCPE 0.8.1 Touch** contra la implementación actual en [`InventoryScreens.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/InventoryScreens.cpp) y [`InventoryScreens.h`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/InventoryScreens.h) de **MCPE 1.0.0-Restored**, identificando exactamente qué partes están mezcladas con el diseño de PC (Java Edition), qué UVs/NinePatches están mal mapeados y cómo adaptarlo fielmente a 1.0.0-Restored.

---

## 1. Arquitectura General de Pantallas: 0.8.1 Touch vs 1.0.0-Restored

| Aspecto | MCPE 0.8.1 Touch | MCPE 1.0.0-Restored (`InventoryScreens.cpp`) | Discrepancia / Mezcla con PC |
| :--- | :--- | :--- | :--- |
| **Flujo de Inventario (Survival)** | Pantallas **separadas e independientes**: `Touch::IngameBlockSelectionScreen` -> botón `Craft` abre `WorkbenchScreen(0)` -> botón `Armor` abre `ArmorScreen`. | Una única clase `InventoryScreen` con `mTab` (`0 = Inventory`, `1 = Crafting`, `2 = Armor`). | **Mezcla PC**: En 0.8.1 Touch no existen pestañas de texto superiores `"Inventory \| Crafting \| Armor"`. |
| **Fondo de pantalla (`renderGameBehind`)** | `PaneCraftingScreen::renderGameBehind()` (`L407-409`) y `ArmorScreen::renderGameBehind()` (`L357-359`) retornan **`0` (false)**. `Touch::IngameBlockSelectionScreen` llama `renderBackground(0)` (`L43`). | Todas las clases en `InventoryScreens.h` retornan `renderGameBehind() const override { return true; }` y dibujan `fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010)`. | **Mezcla PC**: El overlay translúcido sobre el mundo es del diseño de PC; en 0.8.1 Touch las pantallas de inventario/crafteo/armadura son opacas o usan `NinePatch` a pantalla completa. |
| **Contenedor principal** | Pantalla completa (`0, 0, width, height`). | Panel centrado de PC `pw = 220, ph = 150` (`drawPanel` con relleno `0xFFC6C6C6` y borde `0xFF555555`). | **Mezcla PC**: El panel gris `220x150` centrado es un placeholder estilo PC Java. |
| **Botón de cerrar** | `PaneCraftingScreen`: `ImageButton closeButton` (`19x19` en `(width - 19, 0)` con icono `{60, 0, 18, 18}`).<br>`IngameBlockSelectionScreen` / `ArmorScreen`: `Touch::TButton backButton` (`38x18` en `(4, 4)` con texto `"Back"`). | `mCloseButton` (`"X"`, `20x20` en `(px + pw - 20, py - 22)`). En `drawMattisCrafting` además se dibuja un sprite estático en `(w - 19, 0)`. | **Mezcla rota**: En `WorkbenchScreen`, `StonecutterScreen` y `InventoryScreen (mTab == 1)` hay **dos** botones de cierre visibles a la vez: el `"X"` clickable flotando en la esquina del panel PC inexistente y el sprite no-clickable `{60, 0, 18, 18}` en `(w - 19, 0)`. |

---

## 2. Análisis de `spritesheet.png`: 0.8.1 (`128x128`) vs 1.0.0 (`256x256`)

Verificación directa sobre los archivos de textura del repositorio:
- **0.8.1**: `MCPE-0.8.1/mcpe_apk/assets/images/gui/spritesheet.png` es **`128 x 128`**.
- **1.0.0**: `assets/resource_packs/vanilla/textures/gui/spritesheet.png` es **`256 x 256`**.

**Detalle crítico de mapeo UV**:
En el `spritesheet.png` (`256x256`) de 1.0.0-Restored, los sprites originales de 0.8.1 **NO** fueron escalados a 2x; permanecen en el **cuadrante superior izquierdo (`128x128`)** con las mismas coordenadas de píxel `(x, y, w, h)` que en 0.8.1. Por lo tanto, al pasar `texW = 256, texH = 256` en [`NinePatchLayer`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/NinePatchLayer.cpp#L3-L14) y `blitScaled`, las coordenadas de píxel de 0.8.1 siguen apuntando al sprite correcto **siempre que se usen los rectángulos correctos**.

### Tabla de Rectángulos (`IntRectangle`) en `spritesheet.png`

| Elemento | UV en 0.8.1 (`128x128`) | Archivo / Línea 0.8.1 | UV actual en `InventoryScreens.cpp` (`256x256`) | Estado en 1.0.0-Restored |
| :--- | :--- | :--- | :--- | :--- |
| **Fondo pantalla completa** | `{0, 0, 16, 16}`, bordes `4, 4` | `PaneCraftingScreen.cpp:316`, `ArmorScreen.cpp:278` | `{0, 0, 16, 16}`, bordes `4, 4, 4, 4` (`bgLayer`) | Correcto en `drawMattisCrafting`, ausente en `Armor` e `Inventory`. |
| **Marco lista recetas (`_D6E06298`)** | `{0, 20, 8, 8}`, bordes `1, 2`, `setExcluded(16)` | `PaneCraftingScreen.cpp:317` | **No existe** (usa `fill(..., 0xFF333333)`) | **Falta**; además `NinePatchLayer` en 1.0.0 no soporta `setExcluded(16)` (omite el quad central). |
| **Tab categoría NO seleccionada** | `{8, 32, 8, 8}`, bordes `2, 2` (`field_1B8`) | `PaneCraftingScreen.cpp:318` | `{28, 32, 8, 8}`, bordes `2, 2, 2, 2` (`tabUnsel`) | **UV incorrecto**: `{28, 32, 8, 8}` es el fondo de fila de `ItemPane`, no de pestaña de categoría. |
| **Tab categoría SELECCIONADA** | `{0, 32, 8, 8}`, bordes `2, 2` (`field_1BC`) | `PaneCraftingScreen.cpp:319` | `{20, 32, 8, 8}`, bordes `2, 2, 2, 2` (`tabSel`) | **UV incorrecto**: `{20, 32, 8, 8}` es el fondo de fila seleccionada de `ItemPane`. |
| **Fila de `ItemPane` (normal / sel)** | `{20, 32, 8, 8}` / `{28, 32, 8, 8}`, bordes `2, 2` | `ItemPane.cpp:26-27` | **No usado en lista** (dibuja `drawSlotGrid` de PC) | Usado por error en `tabSel`/`tabUnsel`. |
| **Iconos de categoría (4x32x32)** | `u = 32 * (v10 / 2)`, `v = 32 * ((v10 & 1) + 2)`, `32x32` | `PaneCraftingScreen.cpp:359-362` | `u = 32 * (i / 2)`, `v = 32 * ((i & 1) + 2)`, `32x32` | UV correcto, pero tamaño de renderizado deformado (`tabSize - 8` x `tabSize - 10`). |
| **Botón `Craft` (`CraftButton`)** | `{112, 0, 8, 67}` (normal) / `{120, 0, 8, 67}` (pressed), bordes `2, 2` | `CraftButton.cpp:16-17` | Usa `Touch::TButton` (`{8, 32, 8, 8}` / `{0, 32, 8, 8}`) | **UV incorrecto** (usa fondo de `TButton` genérico en lugar del `CraftButton` alto). |
| **Botón cerrar (`closeButton`)** | `{60, 0, 18, 18}` en `(width - 19, 0)`, `19x19` | `PaneCraftingScreen.cpp:305-309, 369-374` | `blitScaled(w - 19, 0, 19, 19, 60, 0, 18, 18, 256, 256)` | UV correcto, pero es solo visual (no es un `Button` real clickable). |
| **Slot armadura (`field_1F4`)** | `{0, 32, 8, 8}`, bordes `3, 3`, `20x20` | `ArmorScreen.cpp:279` | **No existe** (usa `drawSlotGrid` `18x18` de PC) | **Falta**. |
| **Marco `InventoryPane` armadura (`field_1F8`)** | `{28, 42, 4, 4}`, bordes `1, 1`, `setExcluded(16)` | `ArmorScreen.cpp:280` | **No existe** | **Falta**. |
| **Caja 3D jugador armadura (`field_1FC`)** | `{0, 20, 8, 8}`, bordes `3, 3` | `ArmorScreen.cpp:281` | **No existe** (usa `fill(..., 0xFF000000)`) | **Falta**. |

---

## 3. Comparación Línea por Línea por Pantalla

### 3.1. `PaneCraftingScreen` / `WorkbenchScreen` / `StonecutterScreen`
Archivos 0.8.1: [`PaneCraftingScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/PaneCraftingScreen.cpp), [`WorkbenchScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/WorkbenchScreen.cpp), [`StonecutterScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/StonecutterScreen.cpp).
Archivos 1.0.0: [`InventoryScreens.cpp:30-82, 199-245`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/InventoryScreens.cpp#L30-L82).

1. **Título superior**:
   - **0.8.1**: No dibuja ningún texto de título en `(8, 6)` sobre el fondo MATTIS.
   - **1.0.0 (`L36`)**: Dibuja `s->drawString(font, title, 8, 6, 0xFF404040)` (`"Crafting (MATTIS)"`, `"Crafting Table"`, `"Stonecutter"`).
2. **Pestañas de categoría (izquierda)**:
   - **0.8.1 (`L337-364`)**: `v2 = max(4, field_194)`, `v7 = (height - 16) / v2`, `posX = 6`, `posY = 6 + v5 * (v7 + 1) - 1`, `width = height = v7`. En `StonecutterScreen` (`L7-8`), `setSingleCategoryAndIcon(1, 5)` fija **`field_194 = 1`** (1 sola pestaña con icono `5`: `u = 64, v = 96`).
   - **1.0.0 (`L38-50`)**: Fuerza `for (int i = 0; i < 4; ++i)` incluso en `StonecutterScreen` (`L239`), aplica clamp `24..48` a `tabSize`, usa `tabSize x (tabSize - 2)` con UVs de `ItemPane` (`{20,32,8,8}`/`{28,32,8,8}`), y no son botones clickables (`ImageButton`/`CategoryButton`).
3. **Lista central de recetas (`ScrollingPane` / `ItemPane`)**:
   - **0.8.1 (`L257-258, 371-380`)**: Rectángulo `field_184` en `minX = v7 + 12`, `minY = 8`, `width = width - (v7 + 12) - 100`, `height = height - 16`. Dibuja el marco `_D6E06298` (`{0, 20, 8, 8}`) y dentro el `ItemPane` con filas horizontales de alto `22px` (`ItemPane.cpp:14`).
   - **1.0.0 (`L53-61`)**: Usa `listY = 20`, `listH = h - 28`, fondo plano `0xFF333333` y una **cuadrícula de PC** `drawSlotGrid(s, listX + 4, listY + 4, cols, rows, 22)`.
4. **Botón `Craft` e ingredientes (derecha)**:
   - **0.8.1 (`L260-296, 365-367`)**: `craftButton` en `(width - 95, 20)` de tamaño `88x62`. **Dentro** de ese botón `88x62` se renderizan los ingredientes (hasta 4 en cuadrícula 2x2 de `v9 = 44, v10 = 31`) con sus contadores `"tengo/necesito"` a escala `0.66667`. **Debajo** del botón (`posY + height + 6 = 88`), se dibuja la descripción `field_80` con `drawWordWrap`. No existe texto `"Craft"` ni texto `"Ingredients:"`.
   - **1.0.0 (`L64-76`)**: Crea un `static Touch::TButton craftBtn(99, "Craft")` no registrado en `buttons`, llama `craftBtn.render(nullptr, -1, -1)` (nunca recibe hover/click), dibuja `"Craft"` centrado encima, y debajo dibuja `"Ingredients:"` + una **cuadrícula PC 3x1** (`drawSlotGrid(s, craftX, craftY + 82, 3, 1, 20)`).
5. **Retorno al cerrar (`closeScreen`)**:
   - **0.8.1 (`L95-101`)**: Si `field_17C == 0` (crafteo 2x2 abierto desde inventario), vuelve a `INVENTORY_SCREEN`. Si `field_17C != 0` (mesa de crafteo / cortapiedras), cierra la pantalla (`setScreen(0)`).
   - **1.0.0 (`L219-221, 243-245`)**: `WorkbenchScreen` y `StonecutterScreen` llaman `resumeGame()`.

---

### 3.2. `Touch::IngameBlockSelectionScreen` vs `InventoryScreen (mTab == 0)` y `CreativeInventoryScreen`
Archivos 0.8.1: [`Touch_IngameBlockSelectionScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/Touch_IngameBlockSelectionScreen.cpp).
Archivos 1.0.0: [`InventoryScreens.cpp:86-197`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/InventoryScreens.cpp#L86-L197).

1. **Header y botones superiores (0.8.1 `L67-76, 100-135`)**:
   - `Touch::THeader` `"Select blocks"` a ancho completo (`0, 0, width, 26`) usando `gui/touchgui.png` (`150, 26`, `153, 26`, `162, 26`, `153, 52`).
   - `backButton` (`"Back"`, `38x18` en `4, 4`).
   - En Survival (`!isCreativeMode()`): `craftButton` (`"Craft"`, `38x18` en `width - 84, 4`) y `armorButton` (`"Armor"`, `38x18` en `width - 42, 4`).
   - **1.0.0 (`L86-104, 112-115`)**: En su lugar usa 3 pestañas de texto PC (`"Inventory"`, `"Crafting"`, `"Armor"` de `64x20`) + `"X"` (`20x20`) sobre un panel gris `220x150`, y dentro de `mTab == 0` dibuja **cuadrículas PC `9x3` y `9x1` de `18x18`**.
2. **Panel de ítems y Hotbar (0.8.1 `L40-52, 78-99`)**:
   - Usa `Touch::InventoryPane` con celdas de `32x32` + `4px` de separación, `minY = 30`, `height = height - 56`, centrado horizontalmente (`cols = (width - 20) / 32`).
   - Superpone `gui/itemframe.png` (`L47-49`) y dibuja la hotbar real (`gui.renderToolBar`) abajo.
   - **1.0.0 (`CreativeInventoryScreen`, `L146-180`)**: Usa el mismo panel PC `220x150` con 4 botones de texto superiores (`"Building"`, `"Decor"`, `"Tools"`, `"Misc"`) y cuadrículas PC `9x5` + `9x1`.

---

### 3.3. `ArmorScreen` vs `InventoryScreen (mTab == 2)`
Archivos 0.8.1: [`ArmorScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/ArmorScreen.cpp).
Archivos 1.0.0: [`InventoryScreens.cpp:118-124`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/InventoryScreens.cpp#L118-L124).

1. **Layout completo 0.8.1 (`L262-340`)**:
   - Fondo NinePatch `{0, 0, 16, 16}` a pantalla completa.
   - `Touch::THeader` `"Armor"` (`0, 0, width, 26`) + `backButton` `"Back"` (`4, 4, 38, 18`).
   - **Izquierda**: `Touch::InventoryPane` filtrado solo a piezas de armadura (`ItemInstance::isArmorItem`), en `minX = 10, minY = 32, width = ((int)(width / 1.8) - 20) & ~31, height = height - 40`, con borde `{28, 42, 4, 4}`.
   - **Centro**: 4 `BlankButton` (`20x20`) en `posX = pane.width + 20`, `posY = 32 + 24 * i`, dibujados con `{0, 32, 8, 8}` e icono vacío de `items-opaque.png` (`240, 16 * i`).
   - **Derecha**: Caja NinePatch `{0, 20, 8, 8}` con el modelo 3D del jugador (`renderPlayer`).
2. **Implementación actual 1.0.0 (`L118-124`)**:
   - Todo metido dentro del panel PC `220x150`:
     - `drawSlotGrid(this, px + 16, py + 22, 1, 4)` (4 slots PC `18x18` a la izquierda).
     - `fill(px + 44, py + 22, px + 96, py + 94, 0xFF000000)` + texto `"Player"` (caja negra estática).
     - `drawSlotGrid(this, px + 28, py + 102, 9, 2)` (cuadrícula PC `9x2` abajo).

---

## 4. Guía de Adaptación Limpia a MCPE 1.0.0-Restored

1. **Separar las pantallas Touch en `InventoryScreens.h/.cpp`**:
   - Convertir `InventoryScreen` en la pantalla Touch de selección/inventario (`Touch::THeader("Select blocks")`, `Touch::TButton("Back")`, `Touch::TButton("Craft")`, `Touch::TButton("Armor")`, `renderGameBehind() -> false`).
   - Añadir `ArmorScreen` como clase propia (o estado separado a pantalla completa sin el panel PC `220x150`), usando `Touch::THeader("Armor")` + `Touch::TButton("Back")`.
   - En `WorkbenchScreen(bool is2x2 = false)` y `StonecutterScreen`, usar pantalla completa (`renderGameBehind() -> false`), eliminar `fillGradient` y el panel `pw=220, ph=150`, y unificar el botón de cierre en un único `ImageButton` clickable de `19x19` en `(width - 19, 0)` con UV `{60, 0, 18, 18}`.
2. **Corregir `NinePatchLayer` y UVs de `spritesheet.png` (`256x256`)**:
   - Añadir soporte opcional de `excludeCenter` (`setExcluded(16)` de 0.8.1) en [`NinePatchLayer`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/NinePatchLayer.cpp#L48) para poder dibujar los marcos huecos `{0, 20, 8, 8}` (crafteo) y `{28, 42, 4, 4}` (armadura) alrededor de las listas sin tapar su contenido.
   - Cambiar `tabSel` / `tabUnsel` a `{0, 32, 8, 8}` y `{8, 32, 8, 8}` (256x256), y reservar `{20, 32, 8, 8}` / `{28, 32, 8, 8}` exclusivamente para las filas de `ItemPane`.
   - Añadir el `NinePatchLayer` del `CraftButton`: `{112, 0, 8, 67}` (normal) y `{120, 0, 8, 67}` (pressed), bordes `2, 2`, `256x256`.
3. **Reemplazar cuadrículas PC en `drawMattisCrafting`**:
   - Soportar parámetro `numCategories` (`4` para `WorkbenchScreen`, `1` con icono índice `5` para `StonecutterScreen`).
   - Reemplazar `drawSlotGrid` central por una lista vertical de filas de `22px` de alto con fondo `{20, 32, 8, 8}` / `{28, 32, 8, 8}`.
   - Reemplazar `"Craft"` + `"Ingredients:"` + `3x1 drawSlotGrid` por la cuadrícula 2x2 de ingredientes **dentro** del botón `88x62` y la descripción `drawWordWrap` **debajo** del botón (`craftY + 68`).
