#include "Options.h"
#include <algorithm>
#include <sstream>

float Options::getProgressValue(Option opt) const {
	switch (opt) {
	case Option::MUSIC: return musicVolume;
	case Option::SOUND: return soundVolume;
	case Option::SENSITIVITY: return sensitivity;
	case Option::PIXELS_PER_MILLIMETER: return dpadSize;
	default: return 0.0f;
	}
}

void Options::set(Option opt, float value) {
	value = std::max(0.0f, std::min(1.0f, value));
	switch (opt) {
	case Option::MUSIC: musicVolume = value; break;
	case Option::SOUND: soundVolume = value; break;
	case Option::SENSITIVITY: sensitivity = value; break;
	case Option::PIXELS_PER_MILLIMETER: dpadSize = value; break;
	default: break;
	}
}

int Options::getIntValue(Option opt) const {
	switch (opt) {
	case Option::RENDER_DISTANCE: return renderDistance;
	case Option::GUI_SCALE: return guiScale;
	case Option::DIFFICULTY: return difficulty;
	default: return 0;
	}
}

void Options::set(Option opt, int value) {
	switch (opt) {
	case Option::RENDER_DISTANCE: renderDistance = std::max(0, std::min(3, value)); break;
	case Option::GUI_SCALE: guiScale = std::max(1, std::min(4, value)); break;
	case Option::DIFFICULTY: difficulty = std::max(0, std::min(3, value)); break;
	default: break;
	}
}

bool Options::getBooleanValue(Option opt) const {
	switch (opt) {
	case Option::INVERT_MOUSE: return invertMouse;
	case Option::VIEW_BOBBING: return viewBobbing;
	case Option::FANCY_GRAPHICS: return fancyGraphics;
	case Option::HIDE_GUI: return hideGUI;
	case Option::THIRD_PERSON: return thirdPerson;
	case Option::USE_TOUCH_UI: return useTouchUI;
	case Option::SERVER_VISIBLE: return serverVisible;
	case Option::LEFT_HANDED: return leftHanded;
	case Option::USE_TOUCH_JOYPAD: return useTouchJoypad;
	case Option::DESTROY_VIBRATION: return destroyVibration;
	case Option::FANCY_SKIES: return fancySkies;
	case Option::ANIMATE_TEXTURES: return animateTextures;
	default: return false;
	}
}

void Options::toggle(Option opt) {
	switch (opt) {
	case Option::INVERT_MOUSE: invertMouse = !invertMouse; break;
	case Option::VIEW_BOBBING: viewBobbing = !viewBobbing; break;
	case Option::FANCY_GRAPHICS: fancyGraphics = !fancyGraphics; break;
	case Option::HIDE_GUI: hideGUI = !hideGUI; break;
	case Option::THIRD_PERSON: thirdPerson = !thirdPerson; break;
	case Option::USE_TOUCH_UI: useTouchUI = !useTouchUI; break;
	case Option::SERVER_VISIBLE: serverVisible = !serverVisible; break;
	case Option::LEFT_HANDED: leftHanded = !leftHanded; break;
	case Option::USE_TOUCH_JOYPAD: useTouchJoypad = !useTouchJoypad; break;
	case Option::DESTROY_VIBRATION: destroyVibration = !destroyVibration; break;
	case Option::FANCY_SKIES: fancySkies = !fancySkies; break;
	case Option::ANIMATE_TEXTURES: animateTextures = !animateTextures; break;
	default: break;
	}
}

std::string Options::getMessage(Option opt) const {
	std::ostringstream ss;
	switch (opt) {
	case Option::MUSIC:
		ss << "Music: " << (int)(musicVolume * 100.0f) << "%";
		return ss.str();
	case Option::SOUND:
		ss << "Sound: " << (int)(soundVolume * 100.0f) << "%";
		return ss.str();
	case Option::SENSITIVITY:
		ss << "Sensitivity: " << (int)(sensitivity * 100.0f) << "%";
		return ss.str();
	case Option::INVERT_MOUSE:
		return std::string("Invert Mouse: ") + (invertMouse ? "ON" : "OFF");
	case Option::VIEW_BOBBING:
		return std::string("View Bobbing: ") + (viewBobbing ? "ON" : "OFF");
	case Option::FANCY_GRAPHICS:
		return std::string("Fancy Graphics: ") + (fancyGraphics ? "ON" : "OFF");
	case Option::RENDER_DISTANCE: {
		static const char* names[] = { "Tiny", "Short", "Normal", "Far" };
		int idx = std::max(0, std::min(3, renderDistance));
		return std::string("Render Distance: ") + names[idx];
	}
	case Option::GUI_SCALE:
		ss << "GUI Scale: " << guiScale << "x";
		return ss.str();
	case Option::DIFFICULTY: {
		static const char* names[] = { "Peaceful", "Easy", "Normal", "Hard" };
		int idx = std::max(0, std::min(3, difficulty));
		return std::string("Difficulty: ") + names[idx];
	}
	case Option::HIDE_GUI:
		return std::string("Hide GUI: ") + (hideGUI ? "ON" : "OFF");
	case Option::THIRD_PERSON:
		return std::string("Third Person: ") + (thirdPerson ? "ON" : "OFF");
	case Option::USE_TOUCH_UI:
		return std::string("UI Style: ") + (useTouchUI ? "Touch" : "PC");
	case Option::SERVER_VISIBLE:
		return std::string("Local Server Multiplayer: ") + (serverVisible ? "ON" : "OFF");
	case Option::LEFT_HANDED:
		return std::string("Lefty: ") + (leftHanded ? "ON" : "OFF");
	case Option::USE_TOUCH_JOYPAD:
		return std::string("Use Touch Joypad: ") + (useTouchJoypad ? "ON" : "OFF");
	case Option::PIXELS_PER_MILLIMETER:
		ss << "D-Pad Size: " << (int)(dpadSize * 100.0f) << "%";
		return ss.str();
	case Option::DESTROY_VIBRATION:
		return std::string("Feedback: ") + (destroyVibration ? "ON" : "OFF");
	case Option::FANCY_SKIES:
		return std::string("Beautiful Skies: ") + (fancySkies ? "ON" : "OFF");
	case Option::ANIMATE_TEXTURES:
		return std::string("Animated Water: ") + (animateTextures ? "ON" : "OFF");
	}
	return "";
}
