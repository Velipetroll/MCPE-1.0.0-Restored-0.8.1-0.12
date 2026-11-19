#include "GuiContext.h"
#include "Screen.h"
#include "StartMenuScreen.h"
#include "PauseScreen.h"

void GuiContext::setScreen(Screen* screen) {
	if (currentScreen == screen) return;
	delete currentScreen;
	currentScreen = screen;
	if (currentScreen) {
		currentScreen->init(this, guiWidth, guiHeight);
	}
}

void GuiContext::startGame() {
	state = GameState::InGame;
	setScreen(nullptr);
}

void GuiContext::pauseGame() {
	if (state == GameState::InGame) {
		state = GameState::Paused;
		setScreen(new PauseScreen());
	}
}

void GuiContext::resumeGame() {
	if (state == GameState::Paused) {
		state = GameState::InGame;
		setScreen(nullptr);
	}
}

void GuiContext::quitToTitle() {
	state = GameState::StartMenu;
	setScreen(new StartMenuScreen());
}

void GuiContext::requestQuit() {
	shouldQuit = true;
}
