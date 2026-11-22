#pragma once

#include <string>

class Options {
public:
	enum class Option {
		MUSIC,
		SOUND,
		SENSITIVITY,
		INVERT_MOUSE,
		RENDER_DISTANCE,
		VIEW_BOBBING,
		FANCY_GRAPHICS,
		GUI_SCALE,
		DIFFICULTY,
		HIDE_GUI,
		THIRD_PERSON,
		USE_TOUCH_UI,
		SERVER_VISIBLE,
		LEFT_HANDED,
		USE_TOUCH_JOYPAD,
		PIXELS_PER_MILLIMETER,
		DESTROY_VIBRATION,
		FANCY_SKIES,
		ANIMATE_TEXTURES,
		SHOW_FPS,
		FPS_LIMIT,
		GREEDY_MESHING
	};

	float musicVolume = 1.0f;
	float soundVolume = 1.0f;
	float sensitivity = 0.5f;
	bool invertMouse = false;
	int renderDistance = 2;
	bool viewBobbing = true;
	bool fancyGraphics = true;
	int guiScale = 2;
	int difficulty = 2;
	bool hideGUI = false;
	bool thirdPerson = false;
	bool useTouchUI = false;
	std::string username = "Steve";
	bool serverVisible = true;
	bool leftHanded = false;
	bool useTouchJoypad = false;
	float dpadSize = 0.5f;
	bool destroyVibration = true;
	bool fancySkies = true;
	bool animateTextures = true;
	bool showFps = false;
	int fpsLimit = 0; // 0=VSync, 1=30, 2=60, 3=120, 4=240, 5=Unlimited
	bool greedyMeshing = true;

	float getProgressValue(Option opt) const;
	void set(Option opt, float value);

	int getIntValue(Option opt) const;
	void set(Option opt, int value);

	bool getBooleanValue(Option opt) const;
	void toggle(Option opt);

	std::string getMessage(Option opt) const;

	std::string serialize() const;
	void load(const std::string& path);
	void save(const std::string& path) const;
};
