# Investigación Técnica: Menú de Crafteo PC (`useTouchUI == false`) en MCPE 1.0.0-Restored

Este documento detalla los recursos disponibles en el repositorio, la arquitectura de recetas/inventario a portar desde MCPE 0.8.1 y la especificación exacta de layout e interacción por ratón para implementar el menú de crafteo de PC (`useTouchUI == false`) en MCPE 1.0.0-Restored.

---

## 1. Texturas Disponibles en `assets/resource_packs/vanilla/textures/`

### 1.1. Texturas de Contenedores y GUI
Tras inspeccionar todo el árbol `assets/resource_packs/vanilla/textures/gui/`:
- **NO existen** las texturas de Java Edition `gui/container/crafting_table.png`, `gui/container/inventory.png`, `gui/container/furnace.png` ni `gui/container/generic_54.png`.
- **SÍ existen** las siguientes texturas utilizables inmediatamente:
  - [`gui.png`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/assets/resource_packs/vanilla/textures/gui/gui.png) (`256x256` RGBA): atlas clásico (hotbar en `0,0,182,22`, botones clásicos, etc.).
  - [`spritesheet.png`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/assets/resource_packs/vanilla/textures/gui/spritesheet.png) (`256x256` RGBA): atlas Pocket/Touch con los sprites de 0.8.1 en el cuadrante superior izquierdo `128x128`.
  - [`newgui/dialog-background-atlas.png`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/assets/resource_packs/vanilla/textures/gui/newgui/dialog-background-atlas.png) (`128x128` RGBA): fondo de diálogos 1.0.0.
  - [`newgui/arrow_large.png`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/assets/resource_packs/vanilla/textures/gui/newgui/arrow_large.png) (`22x15` RGBA) y [`newgui/anvil-arrow.png`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/assets/resource_packs/vanilla/textures/gui/newgui/anvil-arrow.png) (`22x15` RGBA): **tamaño exacto (`22x15`) de la flecha de crafteo de PC**.
  - [`newgui/classic-button.png`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/assets/resource_packs/vanilla/textures/gui/newgui/classic-button.png), `classic-button-hover.png`, `classic-button-pressed.png` (`8x8` RGBA).
  - Iconos de armadura vacía (`16x16` en `textures/gui/newgui/`):
    - `empty_armor_slot_helmet.png`
    - `empty_armor_slot_chestplate.png`
    - `empty_armor_slot_leggings.png`
    - `empty_armor_slot_boots.png`

### 1.2. Texturas de Ítems y Bloques
- `assets/resource_packs/vanilla/textures/items/` (**371 archivos**) y `assets/resource_packs/vanilla/textures/blocks/` (**399 archivos**).
- En 1.0.0-Restored, [`ItemInstance::getIcon(int frame, bool inInventoryPane)`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/world/item/ItemInstance.cpp#L670-L672) delega a `mItem->getIcon(...)` y retorna `const TextureUVCoordinateSet&`. Para renderizar ítems en slots `18x18` (área interior `16x16`), el renderizador de ítems GUI debe usar `TextureUVCoordinateSet` (o el atlas generado por `TextureAtlas`) en lugar del antiguo `items-opaque.png` de 0.8.1.

### 1.3. Estrategia de Renderizado del Panel PC sin `gui/container/*.png`
Dado que no existen `crafting_table.png` ni `inventory.png`, existen dos opciones limpias:
1. **Renderizado procedural estilo clásico PC (0 dependencias externas)**:
   - Panel `176x166` con bisel clásico: fondo `0xFFC6C6C6`, borde exterior negro `0xFF000000` (o `0xFF555555`), luz superior/izquierda `0xFFFFFFFF` (2px) y sombra inferior/derecha `0xFF555555` (2px).
   - Cada slot `18x18`: borde superior/izquierdo `0xFF373737` (1px), borde inferior/derecho `0xFFFFFFFF` (1px), interior `0xFF8B8B8B` (`16x16`).
   - Flecha de crafteo: `bindTexture("textures/gui/newgui/arrow_large.png")` + `blitScaled(..., 22, 15, 0, 0, 22, 15, 22, 15)`.
2. **Opcional**: Añadir `textures/gui/container/crafting_table.png` e `inventory.png` (`256x256`) y hacer `blit(px, py, 0, 0, 176, 166, 256, 256)`.

---

## 2. Sistema de Recetas, Ítems e Inventario

### 2.1. Qué existe ya en `handheld/src/common/`
- **`world/item/`**: Existen [`Item.h`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/world/item/Item.h) (`MAX_ITEMS = 512`, `mItems[512]`, miembros estáticos con prefijo `m`, ej. `Item::mStick`), [`ItemInstance.h`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/world/item/ItemInstance.h) y `ItemCategory.h`.
- **`world/Container.h`**: Existe [`Container`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/world/Container.h) con `ContainerType::WORKBENCH = 1`, `ContainerType::INVENTORY = -1`, `ContainerID::CONTAINER_ID_INVENTORY = 0`.
- **Qué falta**:
  - `handheld/src/common/world/crafting/` **no existe**.
  - `handheld/src/common/world/inventory/` **no existe** (`Player.cpp` tiene `mInventoryProxy` comentado).
  - [`GuiContext`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/GuiContext.h) no tiene puntero a `Player*` / `Inventory*` todavía.

### 2.2. Qué portar/adaptar desde `MCPE-0.8.1`
De `MCPE-0.8.1/minecraftpe/impl/crafting/` y `headers/crafting/`:
- **`Recipe`** (`Recipe.hpp/.cpp`): Clase base con `SIZE_2X2`, `SIZE_3X3`, `matches(CraftingContainer*)`, `assemble(CraftingContainer*)`, `getResultItem()`, `getCraftingSize()`.
- **`ShapedRecipe`** (`ShapedRecipe.hpp/.cpp`):
  - `matches(CraftingContainer*)` prueba todos los desplazamientos `(i, j)` dentro de la cuadrícula `3x3` (`i <= 3 - width`, `j <= 3 - height`) tanto normal como espejado horizontalmente (`a5 = 1` y `a5 = 0`).
  - Soporta `auxValue == -1` como comodín (cualquier metadata, ej. madera/lana).
- **`ShapelessRecipe`** (`ShapelessRecipe.hpp/.cpp`):
  - Recorre `0..2 x 0..2`, verifica que cada ítem no nulo coincida con algún ingrediente restante en `field_28` (respetando `auxValue == -1`) y que al final `v19.empty()` sea verdadero.
- **`Recipes`** (`Recipes.hpp/.cpp`) y sub-registradores (`ToolRecipes`, `WeaponRecipes`, `OreRecipes`, `FoodRecipes`, `StructureRecipes`, `ArmorRecipes`, `ClothDyeRecipes`):
  - Adaptación necesaria: en 0.8.1 usan `Tile::*` e `Item::stick`, mientras que en 1.0.0-Restored deben apuntar a `Block::*` e `Item::m*` (ej. `Item::mStick`).
- **`CraftingContainer`**:
  - En 0.8.1 `CraftingContainer::getItem(int32_t, int32_t)` estaba sin implementar (`printf("not implemented")`) porque Touch usaba MATTIS (`PaneCraftingScreen`).
  - Para PC, implementar `CraftingContainer(int w, int h)` (`2x2` o `3x3`) con `std::vector<ItemInstance> mItems(w * h)`.
  - `ItemInstance* getItem(int x, int y)`: si `x < 0 || y < 0 || x >= mWidth || y >= mHeight` o `mItems[y * mWidth + x].isNull()`, retornar `nullptr`. **Esto permite que `ShapedRecipe::matches` y `ShapelessRecipe::matches` (que iteran `0..2`) funcionen automáticamente tanto en `2x2` como en `3x3` sin duplicar lógica.**

De `MCPE-0.8.1/minecraftpe/impl/inventory/`:
- **`FillingContainer` + `Inventory`**:
  - 36 slots (`0..8` hotbar, `9..35` inventario principal), más 4 slots de armadura (`0 = helmet, 1 = chestplate, 2 = leggings, 3 = boots`).
  - Conectar un `Inventory*` accesible desde `GuiContext` (o `Player`) para que el menú PC lea y escriba slots reales.

---

## 3. Layout y Coordenadas Exactas del Panel PC (`176x166`)

El panel estándar PC mide **`176 x 166`** píxeles GUI, centrado en pantalla:
- `px = (width - 176) / 2`
- `py = (height - 166) / 2`
- Fondo de pantalla: `renderGameBehind() == true` + `fillGradient(0, 0, width, height, 0xA0101010, 0xB0101010)`.

### 3.1. Mesa de Crafteo (`WorkbenchScreen`, 3×3)
Todas las coordenadas son relativas a `(px, py)`:
- **Título**: `"Crafting"` en `(px + 28, py + 6)`, color `0xFF404040`.
- **Cuadrícula de entrada (3×3, 9 slots `18x18`)**:
  - Origen: `(px + 30, py + 17)`.
  - Slot `(col, row)` (`col, row ∈ [0, 2]`): `sx = px + 30 + col * 18`, `sy = py + 17 + row * 18`.
  - Ícono `16x16` en `(sx + 1, sy + 1)`.
- **Flecha (`22x15`)**: `(px + 90, py + 35)` (`textures/gui/newgui/arrow_large.png`).
- **Slot de resultado**:
  - Marco exterior grande (`26x26`): `(px + 120, py + 31)`.
  - Área de ítem/hit-box (`18x18`): `(px + 124, py + 35)` (ícono `16x16` en `px + 125, py + 36`).
- **Etiqueta `"Inventory"`**: `(px + 8, py + 72)`, color `0xFF404040`.
- **Inventario principal (9×3, slots `9..35`)**:
  - Origen: `(px + 8, py + 84)`.
  - Slot `(col, row)` (`col ∈ [0, 8], row ∈ [0, 2]`): `sx = px + 8 + col * 18`, `sy = py + 84 + row * 18`.
- **Hotbar (9×1, slots `0..8`)**:
  - Origen: `(px + 8, py + 142)`.
  - Slot `col` (`col ∈ [0, 8]`): `sx = px + 8 + col * 18`, `sy = py + 142`.

### 3.2. Inventario Survival (`InventoryScreen`, 2×2)
Mismo panel `176x166` en `(px, py)`:
- **Slots de armadura (1×4, `18x18`)**: `(px + 8, py + 8 + i * 18)` para `i = 0..3` (casco, pechera, pantalones, botas).
- **Caja 3D del jugador (`52x70`)**: de `(px + 26, py + 8)` a `(px + 78, py + 78)`, fondo negro `0xFF000000` con borde `0xFF373737`/`0xFFFFFFFF`.
- **Etiqueta `"Crafting"`**: `(px + 97, py + 8)`, color `0xFF404040`.
- **Cuadrícula de entrada (2×2, 4 slots `18x18`)**:
  - Origen: `(px + 98, py + 18)`.
  - Slot `(col, row)` (`col, row ∈ [0, 1]`): `sx = px + 98 + col * 18`, `sy = py + 18 + row * 18`.
- **Flecha**: `(px + 134, py + 28)` (`arrow_large.png` a `16x15` o `22x15` ajustado).
- **Slot de resultado (`18x18`)**: `(px + 154, py + 28)` (o `(px + 152, py + 28)` con `2px` de margen derecho).
- **Inventario principal (9×3)** y **Hotbar (9×1)**: idénticos a `WorkbenchScreen` (`(px + 8, py + 84)` y `(px + 8, py + 142)`).

---

## 4. Mecánica Completa de Interacción con Ratón

### 4.1. Estado de Cursor y Renderizado
- Mantener `ItemInstance mCarriedItem` (inicialmente nulo/vacío) en la pantalla activa.
- **Hover Highlight**: Para el slot bajo `(mouseX, mouseY)` (si `mouseX >= sx && mouseX < sx + 18 && mouseY >= sy && mouseY < sy + 18`), dibujar encima del ítem:
  ```cpp
  fill(sx + 1, sy + 1, sx + 17, sy + 17, 0x80FFFFFF);
  ```
- **Item flotante bajo el cursor**: Al final de `render(mouseX, mouseY, partialTicks)` (después de todos los slots y botones), si `!mCarriedItem.isNull()`, renderizar su ícono `16x16` en `(mouseX - 8, mouseY - 8)` y su contador si `count > 1` en la esquina inferior derecha.

### 4.2. Clic Izquierdo (`button == 0`) y Clic Derecho (`button == 1`)
En [`Screen::mouseClicked(int x, int y, int button)`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/gui/Screen.cpp#L55-L71), `button == 0` es clic izquierdo y `button == 1` es clic derecho.

| Caso | Clic Izquierdo (`button == 0`) | Clic Derecho (`button == 1`) |
| :--- | :--- | :--- |
| **Slot normal vacío + cursor con ítem** | Mueve todo `mCarriedItem` al slot y limpia `mCarriedItem`. | Coloca `1` unidad de `mCarriedItem` en el slot (`mCarriedItem.remove(1)`). |
| **Slot normal con ítem + cursor vacío** | Mueve todo el stack del slot a `mCarriedItem` y limpia el slot. | Toma la mitad superior `ceil(slot.count / 2)` a `mCarriedItem`; deja `floor(slot.count / 2)` en el slot. |
| **Slot normal con ítem + cursor con el MISMO ítem** (`sameItem` + mismo `aux` + `hasSameUserData`) | Suma hasta `maxStackSize` en el slot; el excedente queda en `mCarriedItem`. | Añade `1` unidad al slot si `slot.count < maxStackSize` y resta `1` a `mCarriedItem`. |
| **Slot normal con ítem + cursor con OTRO ítem** | Intercambia (`std::swap`) el contenido del slot y `mCarriedItem`. | Intercambia (`std::swap`) el contenido del slot y `mCarriedItem`. |
| **Slot de resultado (Crafteo)** | No permite depositar ítems. Si `!result.isNull()` y (`mCarriedItem.isNull()` o (`mCarriedItem == result && mCarriedItem.count + result.count <= maxStackSize`)): suma `result` a `mCarriedItem` y consume ingredientes. | Igual que clic izquierdo (toma 1 crafteo). |

### 4.3. Evaluación de Receta y Consumo de Ingredientes
1. **`updateCraftingResult()`**: Se ejecuta cada vez que cambia cualquier slot de la cuadrícula de entrada (`2x2` o `3x3`):
   - Recorre `Recipes::getInstance()->getRecipes()`.
   - Si `recipe->matches(&mCraftGrid)` es verdadero, copia `recipe->getResultItem()->at(0)` en `mResultSlot`. Si ninguna coincide, `mResultSlot.setNull()`.
2. **`consumeCraftingIngredients()`**: Se ejecuta al tomar `mResultSlot`:
   - Por cada celda `(x, y)` de `mCraftGrid`: si `!slot.isNull()`, decrementa `slot.count -= 1` (si llega a `0`, `slot.setNull()`).
   - *(Opcional cubo de leche/lava)*: si el ingrediente tiene `craftingRemainingItem` (ej. `bucket`), reemplazar por cubo vacío en lugar de borrar.
   - Llamar inmediatamente a `updateCraftingResult()` por si quedan materiales para otro crafteo consecutivo.
3. **Cierre de pantalla (`ESC` / `E` / botón cerrar)**:
   - Devolver `mCarriedItem` y todos los slots no nulos de `mCraftGrid` al inventario del jugador (o dropear al mundo si está lleno) para evitar pérdida de ítems.
