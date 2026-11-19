# Investigación del Sistema GUI/UI en MCPE 0.8.1 y Plan de Adaptación a 1.0.0

Este documento resume la arquitectura de interfaz gráfica (GUI/UI) en `MCPE-0.8.1/minecraftpe/` y detalla los requisitos y pasos para adaptar sus componentes a `MCPE-1.0.0-Restored` (`handheld/`).

---

## 1. Arquitectura Base de GUI

### Jerarquía de Clases
- **`GuiComponent`** ([`MCPE-0.8.1/minecraftpe/headers/gui/GuiComponent.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/GuiComponent.hpp))
  - Clase base para cualquier elemento dibujable en 2D.
  - Miembro: `float zLayer`.
  - Métodos clave: `blit(...)`, `drawRect(...)`, `fill(...)`, `fillGradient(...)`, `fillHorizontalGradient(...)`, `drawString(...)`, `drawCenteredString(...)`.
- **`GuiElement`** ([`MCPE-0.8.1/minecraftpe/headers/gui/GuiElement.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/GuiElement.hpp))
  - Hereda de `GuiComponent`.
  - Miembros: `bool_t active, visible`, `int32_t posX, posY, width, height`, `uint32_t color`, `NinePatchLayer* field_1C`.
  - Métodos virtuales: `tick(Minecraft*)`, `render(Minecraft*, int32_t, int32_t)`, `topRender(...)`, `setupPositions()`, `mouseClicked(...)`, `mouseReleased(...)`, `focusuedMouseClicked(...)`, `focusuedMouseReleased(...)`, `keyPressed(...)`, `keyboardNewChar(...)`, `backPressed(...)`, `pointInside(int32_t, int32_t)`, `suppressOtherGUI()`, `setTextboxText(...)`.
- **`Screen`** ([`MCPE-0.8.1/minecraftpe/headers/gui/Screen.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/Screen.hpp), [`Screen.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/Screen.cpp))
  - Hereda de `GuiComponent`.
  - Miembros: `width`, `height`, `Minecraft* minecraft`, `std::vector<Button*> buttons`, `std::vector<GuiElement*> elements`, `Font* font`, `Button* lastPressedButton`.
  - Ciclo de vida:
    - `init(Minecraft*, int32_t, int32_t)` -> asigna `minecraft`, `width`, `height`, `font`, llama a `init()`, `setupPositions()` y `updateTabButtonSelection()`.
    - `tick()` -> llama a `tick(minecraft)` en cada `GuiElement` de `elements`.
    - `render(int32_t x, int32_t y, float)` -> si `supppressedBySubWindow()` es true, llama a `topRender` de los elementos; si no, llama a `render` de `elements` y luego a `render` de cada `Button` en `buttons` (si `!b->isOverrideScreenRendering()`).
  - Manejo de input:
    - `updateEvents()` -> procesa colas de `Mouse::next()`, `Keyboard::_inputs` y `Keyboard::_inputText`.
    - `mouseEvent()` -> convierte coordenadas de ventana (`minecraft->field_1C`, `minecraft->field_20`) a coordenadas GUI (`width`, `height`) y llama a `mouseClicked` o `mouseReleased`.
    - `mouseClicked` / `mouseReleased` -> propaga a `elements` y verifica `buttons` (`b->clicked(...)`, `b->setPressed()`, `buttonClicked(lastPressedButton)`, sonido `"random.click"`).
    - `keyboardEvent()` / `keyPressed(int32_t)` -> propaga a `elements` y maneja navegación por teclado/mando (`keyMenuNext`, `keyMenuPrevious`, `keyMenuOk`) cuando `!minecraft->useTouchscreen()`.
- **`Button`** ([`MCPE-0.8.1/minecraftpe/headers/gui/buttons/Button.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/buttons/Button.hpp), [`Button.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/buttons/Button.cpp))
  - Hereda de `GuiElement`.
  - **Estados y texturas/UVs**:
    - `Button` base: usa `"gui/gui.png"`. `getYImage(bool_t hovered)` devuelve `0` (disabled: `!active`), `1` (normal: `active && !hovered`), o `2` (hover/pressed: `active && hovered`, o `this->text != 0`). La coordenada Y en la textura es `20 * getYImage(...) + 46`, dibujando dos mitades de ancho `width/2` (desde X=`0` y X=`200 - width/2`, alto `20`).
    - `Touch::TButton` ([`Touch_TButton.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/buttons/Touch_TButton.cpp)): usa `"gui/spritesheet.png"` con `NinePatchLayer` (`field_2C` normal: `IntRectangle(8, 32, 8, 8)`; `field_30` pressed: `IntRectangle(0, 32, 8, 8)`). Cambia a `field_30` cuando `active && pressed` y el puntero está dentro del botón.
    - `ImageButton` / `OptionButton` ([`OptionButton.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/buttons/OptionButton.cpp)): `OptionButton` hereda de `ImageButton`, soporta opciones booleanas (`toggle` -> `options->toggle(...)`) o flotantes (`minValue`/`maxValue`), y actualiza su imagen mediante `updateImage(Options*)`.
  - Subclases notables: `Touch::TButton`, `Touch::THeader`, `ImageButton`, `ImageWithBackground`, `LargeImageButton`, `OptionButton`, `SmallButton`, `BlankButton`, `BuyButton`, `CategoryButton`, `CraftButton`, `Spinner`.
- **`Slider`** ([`Slider.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/elements/Slider.hpp), [`Slider.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/elements/Slider.cpp))
  - Hereda de `GuiElement`. Soporta dos modos según `isStepSliderMaybe`:
    - **Continuo (`isStepSliderMaybe == 0`)**: rango `[minValue, maxValue]`, inicializado desde `options.getProgressValue(option)`.
    - **Por pasos (`isStepSliderMaybe == 1`)**: vector discreto de enteros (`field_28`), inicializado desde `options.getIntValue(option)`.
  - **Estados e interacción**:
    - `mouseClicked` activa `field_34 = 1` si `pointInside(x, y)`.
    - `tick` y `render` actualizan `progress = clamp((mouseX - posX) / width, 0.0f, 1.0f)` mientras `field_34 == 1`, llamando a `setOption(mc)` en `tick`.
    - `mouseReleased` desactiva `field_34 = 0`; en modo por pasos ajusta `progress` al paso más cercano (`field_40`, `field_3C = field_28[v7]`) y llama a `setOption(mc)`.
  - **Renderizado**:
    - Dibuja la barra base con `fill(posX + 5, posY + 7, posX + width - 5, posY + 10, -9408400)`.
    - Si `isStepSliderMaybe == 1`, dibuja las marcas de cada paso con `fill(..., -7303024)`.
    - Dibuja el tirador usando `"gui/touchgui.png"` con `blit(posX + 5 + (int)((width - 10) * progress) - 5, posY, 225, 125, 11, 17, 11, 17)`.
  - **Modificación de opciones**: `setOption(Minecraft*)` llama a `options->set(option, intValue)` en modo pasos o `options->set(option, minValue + (maxValue - minValue) * progress)` en modo continuo.
- **`Font`** ([`MCPE-0.8.1/minecraftpe/headers/rendering/Font.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/rendering/Font.hpp))
  - Constructor: `Font(AppPlatform*, Options*, const std::string&, Textures*)`.
  - Métodos: `draw`, `drawShadow`, `drawSlow`, `drawTransformed`, `drawWordWrap`, `width`, `height`, `charWidth`, `setDefaultTexture`, `setUnicodeTexture`.
- **`NinePatchLayer`** ([`MCPE-0.8.1/minecraftpe/headers/gui/NinePatchLayer.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/NinePatchLayer.hpp))
  - Hereda de `GuiElement`. Renderiza bordes escalables en 9 quads (`CachedQuad cachedQuads[9]`) usando `Tesselator&` y `Textures*`.

---

## 2. Menú Principal

- **`StartMenuScreen`** ([`StartMenuScreen.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/screens/StartMenuScreen.hpp))
  - Versión clásica (no táctil). Botones: `startGameButton`, `joinGameButton`, `optionsButton`, `createButton`, `buyButton`. Cadenas: `mojangABMaybe`, `gameVersion`.
- **`Touch::StartMenuScreen`** ([`Touch_StartMenuScreen.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/screens/Touch_StartMenuScreen.hpp))
  - Versión táctil principal en 0.8.1.
  - Botones: `Touch::TButton playButton`, `Touch::TButton playOnRealmsButton`, `ImageWithBackground* settingsButtonMaybe`, `BuyButton buyButton`.
  - Splashes: `static int32_t currentSplash` y `chooseRandomSplash()` usando `extern char_t* gSplashes[]`.
  - Ambos llaman a `this->renderMenuBackground(a4)` para el fondo 3D.

---

## 3. Menú de Pausa

- **`PauseScreen`** ([`PauseScreen.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/screens/PauseScreen.hpp))
  - Hereda de `Screen`.
  - Botones: `backToGameButton`, `quitToTitleButton`, `quitAndCopyMapButton`, `optionsButton`.
  - Elementos: `Label* gameMenuLabel`, `PackedScrollContainer* field_74` (lista de jugadores en red, gestionada con `playerListChanged` / `rebuildPlayerList`).
  - `renderGameBehind()` permite ver el mundo detrás del menú de pausa.

---

## 4. Menú de Configuraciones

- **`OptionsScreen`** ([`OptionsScreen.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/screens/OptionsScreen.hpp))
  - Hereda de `Screen`.
  - Cabecera `Touch::THeader* headerOptions`, botón `Touch::TButton* buttonBack`.
  - Categorías: `std::vector<ImageButton*> field_60` y `std::vector<OptionsPane*> optionPanes` (`OptionsPane* selectedCategory`).
  - Métodos: `createCategoryButtons()`, `generateOptionScreens()`, `selectCategory(int32_t)`.
- **`OptionsPane`** ([`OptionsPane.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/pane/OptionsPane.hpp))
  - Hereda de `PackedScrollContainer`. Agrupa opciones mediante `createOptionsGroup(std::string)`.
- **`Options`** ([`Options.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/Options.hpp))
  - Almacena `musicVolume`, `soundVolume`, `sensitity`, `invertMouse`, `renderDistance`, `viewBobbing`, `limitFramerate`, `graphics`, `leftHanded`, `destroyVibration`, `difficulty`, `hideGUI`, `thirdPerson`, `guiScale`, `username`, `serverVisible`, `useJoypad`, `useTouchscreen`, `fancySkies`, `animateTextures`, y `KeyMapping` para 16 teclas.

---

## 5. Inventario y Pantallas de Bloques

- **`ArmorScreen`** ([`ArmorScreen.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/screens/ArmorScreen.hpp))
  - Hereda de `Screen` y `Touch::IInventoryPaneCallback`.
  - Muestra 4 slots de armadura (`BlankButton field_178[4]`), el modelo del jugador (`renderPlayer(float, float)`) y el panel de inventario (`Touch::InventoryPane*`).
- **`WorkbenchScreen`** ([`WorkbenchScreen.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/screens/WorkbenchScreen.hpp))
  - Hereda de `PaneCraftingScreen` (también existe `StonecutterScreen` heredando de `PaneCraftingScreen`). Filtra recetas mediante `filterRecipe(const Recipe&)`.
- **`FurnaceScreen`** ([`FurnaceScreen.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/screens/FurnaceScreen.hpp))
  - Hereda de `BaseContainerScreen` y `Touch::IInventoryPaneCallback`. Gestiona `FurnaceTileEntity* tileEntity` y `Touch::InventoryPane*`.
- **`ChestScreen`** ([`ChestScreen.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/screens/ChestScreen.hpp))
  - Hereda de `BaseContainerScreen` y `Touch::IInventoryPaneCallback`. Gestiona `ChestTileEntity* tileEntity` con dos `Touch::InventoryPane*` (cofre e inventario del jugador).
- **Amoladora / Grindstone**:
  - **No existe en MCPE 0.8.1** (verificado mediante búsqueda en `MCPE-0.8.1/minecraftpe/`).

---

## 6. Fondo Cúbico 3D (Panorama)

Implementado en [`Screen::renderMenuBackground(float a2)`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/impl/gui/Screen.cpp#L162-L237):
1. Carga las 6 texturas definidas en `panorama_images[]`:
   - `gui/background/panorama_0.png` .. `panorama_5.png`
2. Desactiva `GL_BLEND` (`0xBE2`), `GL_CULL_FACE` (`0xB44`) y `GL_DEPTH_TEST` (`0xB71`).
3. Configura proyección perspectiva: `gluPerspective(120.0, 1.0, 0.05, 10.0)`.
4. Rota la cámara continuamente usando el tiempo acumulado (`dword_D6E05C20 += minecraft->field_D34 * 30`) con oscilación en X (`Mth::sin(...) + 20`) y giro en Y.
5. Dibuja las 6 caras del cubo unitario (`(-1,-1,1)` a `(1,1,1)`) rotando cada cara (`90`, `180`, `-90` en Y; `90`, `-90` en X) con `Tesselator::instance.begin(4)` y `vertexUV`.
6. Superpone un degradado (`fillGradient`) semitransparente (`Color4(1,1,1,0.35)` a `Color4(0,0,0,0.35)`).

---

## 7. HUD (`Gui`)

Definido en [`MCPE-0.8.1/minecraftpe/headers/gui/Gui.hpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/MCPE-0.8.1/minecraftpe/headers/gui/Gui.hpp):
- Hereda de `GuiComponent` y `AppPlatform::Listener`.
- **Hotbar y slots**: `renderToolBar(float, float)`, `renderSlot(int32_t, int32_t, int32_t, float)`, `renderSlotText(...)`, `getSlotIdAt`, `getSlotPos`, `flashSlot`.
- **Vida y burbujas**: `renderHearts()`, `renderBubbles()`.
- **Chat y mensajes**: `std::vector<GuiMessage> chatMessages`, `addMessage`, `displayClientMessage`, `renderChatMessages(...)`, `showTipMessage`, `renderOnSelectItemNameText`.
- **Efectos y otros**: `renderVignette`, `renderSleepAnimation`, `renderProgressIndicator`, `renderDebugInfo`, `outerBreakRingMesh` / `innerBreakRingMesh`.
- **Barra de experiencia**: No existe en 0.8.1 (no hay métodos ni campos de XP en `Gui.hpp`).

---

## 8. Plan de Adaptación a `MCPE-1.0.0-Restored` (`handheld/`)

### Estado actual en `handheld/`
- **Presente**:
  - `Tessellator` (con doble `s`) en [`handheld/src/common/client/renderer/renderer/Tessellator.h`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src/common/client/renderer/renderer/Tessellator.h).
  - Entrypoint Win32/GLFW en [`handheld/src-client/win/main.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src-client/win/main.cpp) y [`renderer.cpp`](file:///home/varzonelp/PROYECTOS/MCPE-1.0.0-Restored/handheld/src-client/win/renderer.cpp).
- **Faltante en `handheld/src/` y `handheld/data/`**:
  - No existen `Font`, `Screen`, `GuiComponent`, `Gui`, `Button`, `NinePatchLayer`, `Options`, ni `MinecraftClient` con pila de pantallas.
  - No existen las texturas `gui.png`, `spritesheet.png` ni `gui/background/panorama_*.png` en `handheld/data/`.

### Pasos de Adaptación para `handheld/src-client/win/`
1. **Puente de Renderizado (`Tesselator` -> `Tessellator`)**:
   - Adaptar las llamadas de `Tesselator::instance` de 0.8.1 al `Tessellator` de 1.0.0 (`handheld/src/common/client/renderer/renderer/Tessellator.h`), reemplazando `glMatrixMode`/`glPushMatrix`/`gluPerspective` por matrices de `glm` y el pipeline de shaders/hal de `handheld/src-deps/Renderer/`.
2. **Sistema de Fuentes (`Font`)**:
   - Portar `Font` (`default8.png` / `glyph_sizes.bin`) o implementar un renderizador de fuente bitmap compatible con el sistema de texturas de `handheld/`.
3. **Texturas GUI**:
   - Incorporar `gui.png`, `spritesheet.png`, `background.png` y `panorama_0..5.png` en `handheld/data/resource_packs/vanilla/`.
4. **Base GUI (`GuiComponent`, `GuiElement`, `Screen`, `Button`, `NinePatchLayer`)**:
   - Portar estas clases a `handheld/src/common/client/gui/` conectando los eventos de ratón/teclado de GLFW (`handheld/src-client/win/main.cpp`) a `Screen::mouseClicked`, `mouseReleased`, `keyPressed` y `keyboardNewChar`.
5. **Estado de Pantallas y `Options`**:
   - Añadir un gestor de pantalla actual (`currentScreen`) y `Options` mínimo en el cliente de `handheld/src-client/win/` para alternar entre el panorama (`StartMenuScreen`), el juego (`Gui` HUD) y `PauseScreen` / `OptionsScreen`.
