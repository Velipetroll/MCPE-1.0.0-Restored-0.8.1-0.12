# Investigación Técnica 1:1: Crafteo e Inventario Touch (`useTouchUI == true`) en MCPE 1.0.0-Restored

Este documento especifica cómo hacer **100% funcional** el menú de crafteo, inventario y armadura de la versión Touch (`useTouchUI == true`) en [`InventoryScreens.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/InventoryScreens.cpp), tomando como referencia 1:1 MCPE 0.8.1 (`PaneCraftingScreen.cpp`, `WorkbenchScreen.cpp`, `StonecutterScreen.cpp`, `ArmorScreen.cpp`, `Touch_IngameBlockSelectionScreen.cpp`, `ItemPane.cpp`, `CraftingFilters.cpp`) e integrándolo con [`CraftingRecipes.h`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/world/crafting/CraftingRecipes.h) y [`GuiContext.h`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/GuiContext.h).

---

## 1. Estado Actual vs Objetivo 1:1 en `InventoryScreens.cpp`

Actualmente, la rama `useTouchUI == false` (PC) ya usa [`Recipes`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/world/crafting/CraftingRecipes.h#L74-L89) y `GuiContext::inventorySlots`, mientras que la rama `isTouchUI(context)` sigue usando **datos falsos**:
- `PaneCraftingScreen`: usa `getRecipesFor(isStonecutter, mCategory)` con listas estáticas (`stoneRecipes`, `cat0..cat3`), cuadrados de color `fill(..., iconColor)` en lugar de texturas de ítems, contadores falsos (`have`/`need` hardcodeados), y `CraftButton` aún no ejecuta ningún crafteo en `buttonClicked`.
- `InventoryScreen` (Touch): dibuja 36 casillas `32x32` vacías y `drawHotbarOverlay` sin los ítems de `context->inventorySlots`.
- `ArmorScreen`: dibuja `totalArmorItems = 8` casillas vacías en `mPane` sin leer `context->inventorySlots` ni `context->armorSlots`, y `ArmorSlotButton` no equipa ni desequipa armadura.

---

## 2. Filtrado Real de Recetas por Estación y Categoría

### 2.1. Filtrado por Estación (`mIs2x2` vs Mesa `3x3` vs `StonecutterScreen`)
En **MCPE 0.8.1**:
1. **`WorkbenchScreen(true)` (Crafteo 2×2 desde `InventoryScreen -> Craft`)**:
   - `PaneCraftingScreen::initCategories()` (`L142-146`) filtra por tamaño: `recipe->getCraftingSize() <= 2` (`CraftingRecipe::SIZE_2X2`), y `WorkbenchScreen::filterRecipe` (`L11-13`) excluye recetas de cortapiedras: `!CraftingFilters::isStonecutterItem(result)`.
2. **`WorkbenchScreen(false)` (Mesa de crafteo 3×3)**:
   - Permite `recipe->getCraftingSize() <= 3` (`SIZE_2X2` y `SIZE_3X3`), excluyendo `!isStonecutterItem(result)`.
3. **`StonecutterScreen()` (`setSingleCategoryAndIcon(1, 5)`)**:
   - En 0.8.1 (`StonecutterScreen.cpp:6-15`), `mNumCategories = 1`, `mSingleIconIndex = 5` (UV `u = 64, v = 96` en `spritesheet.png`), y `filterRecipe` acepta **solo** `isStonecutterItem(result)` (`CraftingFilters.cpp:6-13`: bloques de material `Stone` o `Sand`, excluyendo `lapis_block`, `furnace`, `stonecutter` y `stone_slab` de madera `aux == 2`).
   - *Nota para `CraftingRecipes`*: Añadir un `bool isStonecutterRecipe() const` (o helper `isStonecutterResult(const ItemInstance&)`) en `CraftingRecipes.h/.cpp` y registrar las recetas de cortapiedras (Stone Bricks, Cobble Wall, Sandstone, Chiseled Stone, Quartz Block, Nether Brick, Stone Slabs/Stairs) para que `StonecutterScreen` las filtre dinámicamente.

### 2.2. Agrupación en las 4 Categorías MATTIS (`ItemCategory`)
En **MCPE 0.8.1** (`PaneCraftingScreen.cpp:40-43, 79-85`):
- `intvec1` almacena la máscara de bits `1 << catIndex` (`1, 2, 4, 8`), que coincide **1:1** con [`ItemCategory`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/world/item/ItemCategory.h#L3-L13) de 1.0.0-Restored:
  - `cat 0` (`Structures = 1`, icono `0` -> `u=0, v=64`): Bloques de construcción, tablones, palos, mesa, horno, cofre, antorcha, cortapiedras, ladrillos.
  - `cat 1` (`Tools = 2`, icono `1` -> `u=0, v=96`): Herramientas, armas y armaduras crafteables (picos, espadas, hachas, palas, arcos, cascos/petos/pantalones/botas).
  - `cat 2` (`FoodArmor = 4`, icono `2` -> `u=32, v=64`): Comida y equipamiento/supervivencia (pan, sopa, tarta, cama, escalera).
  - `cat 3` (`Decorations = 8`, icono `3` -> `u=32, v=96`): Puertas, paneles de cristal, estanterías, tintes, lana, vallas.
- En [`CraftingRecipe`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/world/crafting/CraftingRecipes.h#L30-L40), añadir:
  - `virtual ItemCategory getCategory() const` (o `int categoryMask` en `ShapedRecipe`/`ShapelessRecipe`), y
  - `virtual std::vector<ItemInstance> getCompactIngredients() const` (agrupando ingredientes repetidos de la matriz, ej. `3x Planks + 2x Stick` en lugar de 5 entradas separadas, para que `CraftButton` muestre hasta 4 ingredientes distintos con su suma `need`).

---

## 3. Estado de Crafteabilidad, Ordenamiento y Renderizado de `ItemPane`

### 3.1. Cálculo de Crafteabilidad (`recheckRecipes()`)
En **MCPE 0.8.1** (`PaneCraftingScreen.cpp:170-221`):
1. Recorre `inventorySlots` (`0..35` en 1.0.0-Restored; en 0.8.1 era `9..getContainerSize()-1` porque `0..8` eran linked slots) y suma las cantidades disponibles por `(id, aux)` en un mapa/pack de inventario.
   - Si un ingrediente de la receta tiene `aux == -1` (comodín, ej. cualquier tipo de tablón/lana), suma todos los `aux` (`0..15`) de ese `id` en `inventorySlots`.
2. Para cada receta en la categoría activa:
   - `have` = cantidad disponible en `inventorySlots` de cada ingrediente agrupado.
   - `canBeCrafted` (`field_34`) = `true` si **todos** los ingredientes cumplen `have >= need`.
   - `inventoryCount` (`field_24`) = cuántas unidades del **resultado** ya tiene el jugador en `inventorySlots` (el numerito en la esquina inferior derecha de la fila).

### 3.2. Ordenamiento (`_resortRecipes()`)
En **MCPE 0.8.1** (`PaneCraftingScreen.cpp:46-67`):
- Usa `std::stable_sort` con comparador `_D65FAB7C`:
  1. Primero todas las recetas con `canBeCrafted == true`, después las `false`.
  2. Dentro de cada grupo, orden alfabético por nombre del ítem resultado (`getItemDisplayName`).
- Tras ordenar, mantiene seleccionada la receta `mSelectedRecipe` actual si sigue en la lista (o índice `0`).

### 3.3. Renderizado de cada fila de `ItemPane` (`22px` de alto)
En **MCPE 0.8.1** (`ItemPane.cpp:87-143`):
1. **Fondo de fila**: `rowSel` (`{20, 32, 8, 8}`) si seleccionada, `rowUnsel` (`{28, 32, 8, 8}`) si no.
2. **Icono del ítem resultado (`16x16`)**: A la **derecha** de la fila (`x = mListX + mListW - 18`, `y = ry + 3`), usando `drawPcItemStack` / `getItemTexturePath(result)` (no un cuadrado de color).
3. **Nombre del ítem**: A la izquierda (`x = mListX + 5`):
   - Si `canBeCrafted == true`: `ry + 6`, `font->drawShadow(name, ..., 0xFFF0F0F0)` (o `0xFFFFFFA0` si seleccionada).
   - Si `canBeCrafted == false`: `ry + 7`, color atenuado `0xC0635558` con sombra `0xC0AAAAAA` (exactamente como `ItemPane.cpp:133`).
4. **Cantidad en inventario (`inventoryCount`)**: A escala `0.6667f` en la esquina inferior derecha de la fila (`x = (mListX + mListW - 4 * digits) * 1.5`, `y = (ry + 22 - 8) * 1.5`), en `0xFFF0F0F0` si crafteable o `0xC0635558` si no.
5. **Gradientes de scroll**: Top y bottom `fillGradient(..., 0x44000000, 0)` y `fillGradient(..., 0, 0x44000000)` dentro del scissor (`ItemPane.cpp:147-150`).

---

## 4. Mecánica y Renderizado Exacto de `CraftButton` (`88x62`)

En **MCPE 0.8.1** (`PaneCraftingScreen.cpp:102-129, 260-296` y `InventoryScreens.cpp:493-550`):
1. **Posicionamiento interno (2×2 dentro de `88x62`)**:
   - `v9 = width * 0.5f = 44.0f`, `v10 = height * 0.5f = 31.0f`.
   - Origen base: `v11 = posX + 22.0f - 8.0f`, `v12 = posY + 15.5f - 9.0f`.
   - Para cada ingrediente `i ∈ [0, min(4, reqs.size()) - 1]`:
     - `ix = v11 + v9 * (i & 1)`, `iy = v12 + v10 * (i >> 1)`.
     - Dibujar el **icono real** `16x16` del ingrediente (con `aux = 0` si `aux == -1`) en `(ix, iy)`.
2. **Contadores `"tengo/necesito"`**:
   - Escala `0.66667f` en `tx = ix * 1.5f + 4.0f`, `ty = iy * 1.5f + 23.0f`.
   - Si `have >= need`: `drawShadow(label, tx, ty, 0xFFF0F0F0)` (blanco).
   - Si `have < need`: `draw(label, tx + 1, ty + 1, 0xC0AAAAAA)` + `draw(label, tx, ty, 0xC0635558)` (gris/rojizo apagado de 0.8.1, o `0xFFFF5555` si se prefiere rojo explícito).
3. **Descripción debajo**:
   - En `(craftButton.posX, craftButton.posY + craftButton.height + 6)` con ancho `craftButton.width` (`88px`) usando `drawWordWrap(..., 0xFFF0F0F0)`.
4. **Acción al pulsar `Craft` (`buttonClicked` cuando `button == mCraftButton`)**:
   - Verificar `activeRecipe->canBeCrafted`.
   - Por cada ingrediente agrupado `(reqItem, needCount)`: consumir exactamente `needCount` unidades de `context->inventorySlots` (respetando `aux == -1` si es comodín, restando de los slots que coincidan hasta completar `needCount`, y haciendo `slot.setNull()` si `slot.count == 0`).
   - Añadir `activeRecipe->result` a `context->inventorySlots` mediante `context->addItemToInventory(result)`.
   - Llamar inmediatamente a `recheckRecipes()` para recalcular `have`, `canBeCrafted`, `inventoryCount` y reordenar la lista.

---

## 5. Sincronización con `InventoryScreen` Touch y `ArmorScreen`

### 5.1. `InventoryScreen` Touch (`Touch::IngameBlockSelectionScreen`)
En [`InventoryScreens.cpp:645-684`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/InventoryScreens.cpp#L645-L684):
- **Cuadrícula `32x32`**:
  - En lugar de mostrar 36 casillas vacías, iterar `i = 9..35` (los 27 slots del inventario principal, o `0..35` si se muestran todos; en 0.8.1 Touch muestra `9..containerSize-1` arriba y `0..8` en la hotbar inferior).
  - Para cada slot no nulo `context->inventorySlots[i]`, dibujar su ícono centrado en `(sx + 8, sy + 8)` (o escalado `20x20`/`24x24` dentro de `32x32`) + su contador `count` si `> 1`.
  - Al tocar un slot (`mouseReleased` sin drag), intercambiar/mover el ítem con `context->selectedHotbarSlot` (como `addItem` en `Touch_IngameBlockSelectionScreen.cpp:181-202`: si ya está en la hotbar, seleccionarlo; si no, `std::swap(inventorySlots[selectedHotbarSlot], inventorySlots[clickedSlot])`).
- **Hotbar inferior (`drawHotbarOverlay`)**:
  - Renderizar los ítems reales de `context->inventorySlots[0..8]` y sus contadores dentro de los 9 slots de `gui.png` (`0, 0, 182, 22`) y resaltar `context->selectedHotbarSlot` (`0, 22, 24, 24`).

### 5.2. `ArmorScreen`
En [`InventoryScreens.cpp:835-946`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/InventoryScreens.cpp#L835-L946):
- **Panel izquierdo (`mPane`)**:
  - Filtrar `context->inventorySlots` para listar únicamente los índices donde el ítem es armadura (casco, pechera, pantalones, botas) y guardar su índice de slot original `mArmorInvIndices`.
  - Dibujar los ítems reales filtrados dentro de las celdas `32x32` del `glScissor`.
  - Al hacer clic en una celda (sin drag), determinar el slot de armadura destino (`0..3` según tipo: casco=0, pechera=1, pantalones=2, botas=3), intercambiar `context->armorSlots[targetSlot]` con `context->inventorySlots[invIdx]` (o mover al inventario si ya había una pieza equipada) y reconstruir la lista filtrada.
- **4 `ArmorSlotButton` (`20x20`)**:
  - Si `!context->armorSlots[i].isNull()`, dibujar el ícono `16x16` de `armorSlots[i]` en `(posX + 2, posY + 2)` en lugar del icono vacío `empty_armor_slot_*.png`.
  - Al pulsar `ArmorSlotButton` (`button->id` `10..13` en `ArmorScreen::buttonClicked`), si `!context->armorSlots[i].isNull()`, mover la pieza de vuelta al inventario con `context->addItemToInventory(armorSlots[i])` y limpiar `armorSlots[i].setNull()`.
