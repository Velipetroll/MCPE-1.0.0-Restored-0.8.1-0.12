# Investigación de la UI Touch Real en Runtime (`MCPE-0.8.1`)

Este documento documenta **exclusivamente** las clases y flujos reales en runtime de `MCPE-0.8.1/minecraftpe/` (descartando código muerto/legacy de versiones 0.1.0–0.7.0).

---

## 1. Controles Touch y Estados (`NinePatchLayer`, `spritesheet.png`, `touchgui.png`)

### `Touch::TButton`
- **Archivos**: [`Touch_TButton.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/buttons/Touch_TButton.hpp), [`Touch_TButton.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/buttons/Touch_TButton.cpp)
- **Tamaño por defecto**: `66x26`.
- **Textura y NinePatch**: Usa `NinePatchFactory` sobre `"gui/spritesheet.png"` con bordes simétricos `(edgeX=2, edgeY=2)`:
  - **Normal (`field_2C`)**: `IntRectangle(minX=8, minY=32, width=8, height=8)`.
  - **Pressed (`field_30`)**: `IntRectangle(minX=0, minY=32, width=8, height=8)`.
- **Estados en `renderBg`**: Si `active && pressed` y el puntero está dentro, dibuja `field_30`; en caso contrario, dibuja `field_2C`.

### `Touch::THeader`
- **Archivos**: [`Touch_THeader.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/buttons/Touch_THeader.hpp), [`Touch_THeader.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/buttons/Touch_THeader.cpp)
- **Tamaño por defecto**: `66x26`, `active = 0`.
- **Textura y UVs**: `"gui/touchgui.png"`:
  - Borde izquierdo (`2px`): UV `(150, 26)`, tamaño `(2, 25)`.
  - Centro estirado (`width - 3`): UV `(153, 26)`, tamaño `(8, 25)`.
  - Borde derecho (`2px`): UV `(162, 26)`, tamaño `(2, 25)`.
  - Sombra inferior (`3px` alto): UV `(153, 52)`, tamaño `(8, 3)`.
- **Texto**: `drawCenteredString(mc->font, maybeTextOnButton, ..., posY + (height - 8) / 2, 0xe0e0e0)`.

### `ImageButton` y `OptionButton`
- **Archivos**: [`ImageButton.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/buttons/ImageButton.cpp), [`OptionButton.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/buttons/OptionButton.cpp)
- **`ImageButton`**: Escala `0.95x` en pressed (si `field_64`), tinte `0xff808080` en disabled, desplazamiento U `+image.field_1C` cuando `isSecondImage` es true.
- **`OptionButton`**: Switches ON/OFF en `"gui/touchgui.png"` (`38x20`): OFF UV `(160, 206)`, ON UV `(198, 206)`.

---

## 2. Cálculo de `GuiScale` y Resolución Virtual ([`Minecraft.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/Minecraft.cpp#L860-L895))

- `GuiScale`: `1` (`field_1C < 400`), `2` (`>= 400`), `3` (`>= 800`), `4` (`>= 1000 && ppm <= 15`), `6` (`>= 1000 && ppm > 15`).
- `InvGuiScale = 1.0f / GuiScale`.
- `screen->width = (int32_t)(field_1C * InvGuiScale)`, `screen->height = (int32_t)(field_20 * InvGuiScale)`.

---

## 3. Pantallas Reales en Runtime (`MCPE-0.8.1`)

### `Touch::StartMenuScreen` ([`Touch_StartMenuScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/Touch_StartMenuScreen.cpp))
- **Fondo**: `renderMenuBackground(a4)` (panorama cúbico 3D).
- **Logo (`gui/title.png`)**: `posY = 12`, `v10 = min(width * 0.5f, data->width * 0.5f)`, `logoW = v10 * 2`, `posX = width / 2 - v10`, `logoH = (logoW / data->width) * data->height`.
- **Splash**: `gSplashes[currentSplash]` amarillo (`0xFFFFFF00`) rotado `-20°`.
- **Botones**:
  - `playButton` ("Play") -> abre `PlayScreen(1)`.
  - `playOnRealmsButton` ("Play on Realms") -> abre `PlayScreen(0)`.
  - `settingsButtonMaybe` (`32x32` en `width - 34, height - 34`, icono `gui/touchgui.png` UV `(218, 0, 22, 21)`) -> abre `OptionsScreen(0)`.
  - `buyButton` (solo demo) en `((width - buyButton.width) / 2, height - buyButton.height - 3)`.
- **Copyright y Versión**: `"\x0fMojang AB"` en `(1, height - 10)` y versión en `(1, height - 20)`. *(No existe botón Quit).*

### `PlayScreen`, `CreateWorldScreen`, `Touch::DeleteWorldScreen`, `AddExternalServerScreen`, `ManageMCOServerScreen`
- **`PlayScreen`** ([`PlayScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/PlayScreen.cpp)): Reemplaza a las pantallas legacy de selección de mundo y unión a partidas; lista mundos locales, LAN, servidores externos y Realms.
- **`CreateWorldScreen`** ([`CreateWorldScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/CreateWorldScreen.cpp)): Creación de mundos locales y Realms.
- **`Touch::DeleteWorldScreen`** ([`Touch_DeleteWorldScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/Touch_DeleteWorldScreen.cpp)): Confirmación de borrado, vuelve a `PlayScreen(1)`.
- **`AddExternalServerScreen`** ([`AddExternalServerScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/AddExternalServerScreen.cpp)): Añadir servidor externo (*Name*, *Address*, *Port*), vuelve a `PlayScreen(1)`.
- **`ManageMCOServerScreen`** ([`ManageMCOServerScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/ManageMCOServerScreen.cpp)): Gestión de Realms.

### `OptionsScreen` + `OptionsPane` + `OptionsGroup` + `OptionsItem` ([`OptionsScreen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/screens/OptionsScreen.cpp))
- **Cabecera**: `buttonBack` en `(4, 4)`, `headerOptions` (`Touch::THeader`) en `(0, 0, width, buttonBack->height + 8)`.
- **Barra lateral de categorías**: `posX = 5`, `posY = headerOptions->height + 3 + v3 + 29 * i` (con iconos de `gui/touchgui2.png`).
- **Panel derecho (`OptionsPane` / `PackedScrollContainer`)**: `posX = catW + 20`, `posY = headerOptions->height + 3`, `width = width - catW - 20`, `height = height - headerOptions->height - 3`.
- **Grupos y filas (`OptionsGroup` / `OptionsItem`)**: Título en `(posX + 2, posY)`, filas con espaciado `+3px`, etiqueta `0xFFBBBBBB` a la izquierda, control (`OptionButton` `38x20`, `Slider` `100x20` con perilla `gui/touchgui.png` UV `(225, 125, 11, 17)`, `TextBox` `100x18`) a `posX + width - c->width - 15`.
- **4 Pestañas**:
  1. **Game**: `NAME`, `DIFFICULTY`, `THIRD_PERSON`, `SERVER_VISIBLE`, `LoginOption`.
  2. **Controls**: `SENSITIVITY`, `INVERT_MOUSE`, `LEFT_HANDED`, `USE_TOUCHSCREEN`, `USE_TOUCH_JOYPAD`, `PIXELS_PER_MILLIMETER`, `DESTROY_VIBRATION`.
  3. **Graphics**: `RENDER_DISTANCE`, `GRAPHICS`, `FANCY_SKIES`, `ANIMATE_TEXTURES`, `HIDE_GUI`.
  4. **Sound**: `SOUND`.

### Menús In-Game y Contenedores
- **`PauseScreen`**, **`ChatScreen`**, **`InBedScreen`**, **`DeathScreen`**, **`DisconnectionScreen`**, **`ProgressScreen`**.
- **Inventarios y Bloques**: `CreativeInventoryScreen`, `Touch::IngameBlockSelectionScreen`, `ArmorScreen`, `WorkbenchScreen` (`PaneCraftingScreen`), `StonecutterScreen`, `FurnaceScreen`, `ChestScreen`, `TextEditScreen`.

---

## 4. Sub-variantes (`useTouchscreen()`)
- `Minecraft::useTouchscreen()` devuelve `options.useTouchscreen || !supportsNonTouchScreen`.
- Cuando es `false` (ej. Xperia Play), `Minecraft` usa `XperiaPlayInput`, `Screen.cpp` habilita navegación por foco/teclas y `Gui.cpp` oculta los controles táctiles del HUD.


---

# Auditoría de Texturas GUI y UVs (`1.0.0` vs `MCPE-0.8.1`)

## 1. Comparación de Dimensiones Reales

| Textura | `MCPE-0.8.1` (`mcpe_apk/assets/images/`) | `1.0.0` (`assets/resource_packs/vanilla/`) | Estado |
| :--- | :---: | :---: | :--- |
| `gui/spritesheet.png` | **128×128** | **256×256** | **Discrepancia 2×** |
| `gui/touchgui.png` | 256×256 | 256×256 | Idéntica |
| `gui/touchgui2.png` | 256×256 | 256×256 | Idéntica |
| `gui/gui.png` | 256×256 | 256×256 | Idéntica |
| `gui/title.png` | 512×64 | 512×64 | Idéntica |
| `gui/background/panorama_0..5.png` | 256×256 | 256×256 | Idéntica |
| `font/default8.png` | 128×128 | 128×128 | Idéntica |

## 2. UVs en `handheld/src/common/client/gui/` vs `MCPE-0.8.1`

- **`spritesheet.png` (Desfase 128×128 vs 256×256)**:
  - `TouchControls.cpp` (`TButton`: `{8, 32, 8, 8}`, `{0, 32, 8, 8}`; `ImageWithBackground`: `{112, 0, 8, 67}`, `{120, 0, 8, 67}`) y `PlayScreen.cpp` (`{34, 43, 14, 14}`) pasan `texW=256, texH=256` a `NinePatchLayer` usando las coordenadas en píxeles de `0.8.1` (que eran sobre `128x128`).
  - Si `spritesheet.png` de `1.0.0` está escalada a `256x256`, los UVs normalizados (`u / 256`) apuntan a la mitad de la coordenada esperada (deberían multiplicarse por `2`, o usar `128, 128` como tamaño base de UV si la textura es un atlas 2×).
- **`touchgui.png` (`256x256`)**:
  - `THeader` (`150, 26`, `153, 26`, `162, 26`, `153, 52`), `StartMenuScreen` (`218, 0, 22, 21`) y `Slider` (`225, 125, 11, 17`) coinciden exactamente con `0.8.1`.
- **`touchgui2.png` (`256x256`)**:
  - Coincide en dimensiones (`256x256`) para los iconos de categorías en `OptionsScreen.cpp`.


---

# Inspección Profunda del Menú de Inicio (`StartMenuScreen.cpp` vs `Touch_StartMenuScreen.cpp` 0.8.1)

## 1. Texturas y Sprites
- **Panorama 3D**: Ambos usan `panorama_0..5.png` (256×256) con rotación continua y overlay degradado.
- **Logo (`title.png`, 512×64)**:
  - `0.8.1`: `v10 = min(width * 0.5f, dataW * 0.5f)` (`logoW = v10 * 2`, hasta 100% del ancho si `width < 512`).
  - `1.0.0`: `v10 = min(width * 0.45f, dataW * 0.5f)` (`90%` del ancho máximo en lugar de `100%`).
- **Botones**:
  - `spritesheet.png` en `0.8.1` es **128×128**, mientras que en `1.0.0` es **256×256** (desfase 2× en `NinePatchLayer` si se pasan UVs de 0.8.1 con `texW=256`).
  - `touchgui.png` (256×256): icono de engranaje `(218, 0, 22, 21)` coincide.

## 2. Tamaños y Geometría (`setupPositions()`)
- **Logo**: `y = 12` en ambos.
- **Botones Play / Realms (Touch)**:
  - `0.8.1`: centrados verticalmente en el espacio restante bajo el logo (`v14 = playButton.posY - (height - (12 + logoH) - (realmsBottom - playTop)) / 2`).
  - `1.0.0`: verificar si usa el mismo centrado vertical o un offset fijo.
- **Botón Opciones (Touch)**: `32×32` en `(width - 34, height - 34)` en `0.8.1` vs `(width - 38, height - 46)` en `1.0.0` (desfasado `4px` en X y `12px` en Y).

## 3. Respuesta a `GuiScale` / Resize (`renderer.cpp` vs `Minecraft.cpp`)
- `renderer.cpp::updateGuiScale()` replica exactamente la tabla de `0.8.1` (`1, 2, 3, 4` según `screenx >= 400, 800, 1000`), permitiendo además override manual con `gGuiOptions.guiScale > 0`.
- `framebuffer_size_callback` llama a `updateGuiScale()` y `currentScreen->init(&gGuiContext, guiWidth, guiHeight)` (en `0.8.1` llama a `setSize()` + `setupPositions()`).

## 4. Botones e Interacción (`init()`)
- `0.8.1` (`Touch::StartMenuScreen`): `playButton`, `playOnRealmsButton`, `settingsButtonMaybe`, `buyButton` (demo). **Sin botón `Quit`**.
- `1.0.0` (`StartMenuScreen`): en Touch incluye `Play`, `Realms`, `Options` (engranaje) y **sobra `Quit`**; en PC incluye `Play`, `Realms`, `Options`, `Quit`.

## 5. Textos y Tipografía (`render()`)
- **Splash text**:
  - `0.8.1`: `font->drawTransformed(splash, logoRight * 0.71f, logoBottom - 15, 0xFFFFFF00, -20, powf(sin(t * 3.14 * 2.3), 4.0) * 0.06f + 1.3f, 1, width * 0.3125f)`.
  - `1.0.0`: posición fija `(width / 2 + 88, 66.0f)`, escala base `1.15f` (en lugar de `1.3f`) y sin `maxWidth` (`width * 0.3125f`).
- **Copyright y Versión**:
  - `0.8.1`: ambos en el **borde izquierdo** (`x = 1`): `"\x0fMojang AB"` en `y = height - 10`, versión en `y = height - 20`.
  - `1.0.0`: versión en `(4, height - 12)` y `"Mojang AB"` en la **esquina derecha** `(width - font->width(copy) - 4, height - 12)`.
