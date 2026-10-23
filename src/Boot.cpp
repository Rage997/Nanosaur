// NANOSAUR ENTRY POINT
// (C) 2025 Iliyas Jorio
// This file is part of Nanosaur. https://github.com/jorio/nanosaur

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "Pomme.h"
#include "PommeGraphics.h"
#include "PommeInit.h"
#include "PommeFiles.h"

extern "C"
{
	#include "game.h"

	SDL_Window* gSDLWindow = nullptr;
	WindowPtr gCoverWindow = nullptr;
	UInt32* gBackdropPixels = nullptr;
	FSSpec gDataSpec;
	int gCurrentAntialiasingLevel;
}

static fs::path FindGameData(const char* executablePath)
{
	fs::path dataPath;

	int attemptNum = 0;

#if !(__APPLE__)
	attemptNum++;		// skip macOS special case #0
#endif

	if (!executablePath)
		attemptNum = 2;

tryAgain:
	switch (attemptNum)
	{
		case 0:			// special case for macOS app bundles
			dataPath = executablePath;
			dataPath = dataPath.parent_path().parent_path() / "Resources";
			break;

		case 1:
			dataPath = executablePath;
			dataPath = dataPath.parent_path() / "Data";
			break;

		case 2:
			dataPath = "Data";
			break;

		default:
			throw std::runtime_error("Couldn't find the Data folder.");
	}

	attemptNum++;

	dataPath = dataPath.lexically_normal();

	// Set data spec -- Lets the game know where to find its asset files
	gDataSpec = Pomme::Files::HostPathToFSSpec(dataPath / "System");

	FSSpec someDataFileSpec;
	OSErr iErr = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":System:gamecontrollerdb.txt", &someDataFileSpec);
	if (iErr)
	{
		goto tryAgain;
	}

	return dataPath;
}

static void Boot(int argc, char** argv)
{
	SDL_SetAppMetadata(GAME_FULL_NAME, GAME_VERSION, GAME_IDENTIFIER);
#if _DEBUG
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
#else
	SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
#endif

#ifdef __EMSCRIPTEN__
	// The page restores this IDBFS mount before allowing the game to start.
	setenv("XDG_CONFIG_HOME", "/storage", 1);
	SDL_SetHint(SDL_HINT_EMSCRIPTEN_KEYBOARD_ELEMENT, "#canvas");
#endif

	// Start our "machine"
	Pomme::Init();

	// Find path to game data folder
	const char* executablePath = argc > 0 ? argv[0] : NULL;
	fs::path dataPath = FindGameData(executablePath);

	// Load game prefs before starting
	LoadPrefs();

retryVideo:
	// Initialize SDL video subsystem
	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		throw std::runtime_error(std::string("Couldn't initialize SDL video subsystem: ") + SDL_GetError());
	}

	// Create window
#ifdef __EMSCRIPTEN__
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
#endif
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);

	gCurrentAntialiasingLevel = gGamePrefs.antialiasingLevel;
	if (gCurrentAntialiasingLevel != 0)
	{
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
		SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 1 << gCurrentAntialiasingLevel);
	}

	gSDLWindow = SDL_CreateWindow(
		GAME_FULL_NAME " " GAME_VERSION, 640, 480,
		SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);

	if (!gSDLWindow)
	{
		if (gCurrentAntialiasingLevel != 0)
		{
			SDL_Log("Couldn't create SDL window with the requested MSAA level. Retrying without MSAA...");

			// retry without MSAA
			gGamePrefs.antialiasingLevel = 0;
			SDL_QuitSubSystem(SDL_INIT_VIDEO);
			goto retryVideo;
		}
		else
		{
			throw std::runtime_error("Couldn't create SDL window.");
		}
	}

	// Set up globals that the game expects
	gCoverWindow = Pomme::Graphics::GetScreenPort();
	gBackdropPixels = (UInt32*) GetPixBaseAddr(GetGWorldPixMap(gCoverWindow));

	// Init gamepad subsystem
	SDL_Init(SDL_INIT_GAMEPAD);
	auto gamecontrollerdbPath8 = (dataPath / "System" / "gamecontrollerdb.txt").u8string();
	if (-1 == SDL_AddGamepadMappingsFromFile((const char*)gamecontrollerdbPath8.c_str()))
	{
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_WARNING, GAME_FULL_NAME, "Couldn't load gamecontrollerdb.txt!", gSDLWindow);
	}
}

static void Shutdown()
{
	// Always restore the user's mouse acceleration before exiting.
	// SetMacLinearMouse(false);

	DisposeMovie();
	Pomme::Shutdown();

	if (gSDLWindow)
	{
		SDL_DestroyWindow(gSDLWindow);
		gSDLWindow = NULL;
	}

	SDL_Quit();
}

#ifdef __EMSCRIPTEN__
static bool webSuspended;

extern "C" EMSCRIPTEN_KEEPALIVE void WebSuspend(int suspended)
{
	if (!gSDLWindow || webSuspended == (suspended != 0)) return;
	webSuspended = suspended != 0;
	PauseAllChannels(webSuspended || gGamePaused);
	PauseMovie(webSuspended);
	ClearInputStates();
	QD3D_ResetFrameTimer();
}

extern "C" EMSCRIPTEN_KEEPALIVE void WebResize(int width, int height)
{
	if (!gSDLWindow || width < 1 || height < 1) return;
	SDL_SetWindowSize(gSDLWindow, width, height);
	QD3D_OnWindowResized();
}


extern "C" EMSCRIPTEN_KEEPALIVE void WebClearInput(void)
{
	if (gSDLWindow) ClearInputStates();
}

static void WebFrame(void)
{
	if (webSuspended) return;
	try
	{
		QD3D_CalcFramesPerSecond();
		GameMain();
	}
	catch (Pomme::QuitRequest&)
	{
		emscripten_cancel_main_loop();
		Shutdown();
		EM_ASM({ Module['onExit'](); });
	}
	catch (std::exception& error)
	{
		emscripten_cancel_main_loop();
		EM_ASM({ Module['reportError'](UTF8ToString($0)); }, error.what());
		Shutdown();
	}
	catch (...)
	{
		emscripten_cancel_main_loop();
		EM_ASM({ Module['reportError']('Unknown game error'); });
		Shutdown();
	}
}
#endif

int main(int argc, char** argv)
{
	bool success = true;
	std::string uncaught = "";

	try
	{
		Boot(argc, argv);
		QD3D_ResetFrameTimer();
#ifdef __EMSCRIPTEN__
		emscripten_set_main_loop(WebFrame, 0, false);
		return 0;
#else
		while (true)
		{
			QD3D_CalcFramesPerSecond();
			GameMain();
		}
#endif
	}
	catch (Pomme::QuitRequest&)
	{
		// no-op, the game may throw this exception to shut us down cleanly
	}
#if !(_DEBUG) || defined(__EMSCRIPTEN__)
	// In release builds, catch anything that might be thrown by GameMain
	// so we can show an error dialog to the user.
	catch (std::exception& ex)		// Last-resort catch
	{
		success = false;
		uncaught = ex.what();
	}
	catch (...)						// Last-resort catch
	{
		success = false;
		uncaught = "unknown";
	}
#endif

	Shutdown();

	if (!success)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Uncaught exception: %s", uncaught.c_str());
#ifdef __EMSCRIPTEN__
		EM_ASM({ Module['reportError'](UTF8ToString($0)); }, uncaught.c_str());
#else
		SDL_ShowSimpleMessageBox(0, GAME_FULL_NAME, uncaught.c_str(), nullptr);
#endif
	}

	return success ? 0 : 1;
}
