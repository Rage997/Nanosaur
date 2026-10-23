/****************************/
/*    NANOSAUR - MAIN 	*/
/* (c)1997 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "game.h"


/****************************/
/*    PROTOTYPES            */
/****************************/

static void InitLevel(void);
static void CleanupLevel(void);
static bool PlayLevel(void);


/****************************/
/*    CONSTANTS             */
/****************************/

#define	KILL_DELAY	4


/****************************/
/*    VARIABLES             */
/****************************/


Boolean		gGameOverFlag;
Boolean		gPlayerGotKilledFlag,gWonGameFlag;

QD3DSetupOutputType		*gGameViewInfoPtr = nil;

PrefsType	gGamePrefs;


//======================================================================================
//======================================================================================
//======================================================================================


void InitDefaultPrefs(void)
{
	SDL_memset(&gGamePrefs, 0, sizeof(gGamePrefs));
	SDL_snprintf(gGamePrefs.magic, sizeof(gGamePrefs.magic), "%s", PREFS_MAGIC);

	gGamePrefs.highQualityTextures = true;			// set the defaults
	gGamePrefs.canDoFog = true;
	gGamePrefs.shadows = true;
	gGamePrefs.dust = true;
	gGamePrefs.fullscreen = true;
	gGamePrefs.displayNum = 0;
	gGamePrefs.antialiasingLevel = 0;
	gGamePrefs.vsync = true;
	gGamePrefs.mainMenuHelp = true;
	gGamePrefs.extreme = false;
	gGamePrefs.music = true;
	gGamePrefs.ambientSounds = true;
	gGamePrefs.nanosaurTeethFix = true;
	gGamePrefs.whiteSky = true;

	SDL_memcpy(gGamePrefs.keys, kDefaultKeyBindings, sizeof(gGamePrefs.keys));
	_Static_assert(sizeof(kDefaultKeyBindings) == sizeof(gGamePrefs.keys), "size mismatch: default keybindings / prefs keybinings");
}

/*****************/
/* TOOLBOX INIT  */
/*****************/

void ToolBoxInit(void)
{
OSErr		iErr;

//	gMainAppRezFile = CurResFile();


			/* MAKE FSSPEC FOR DATA FOLDER */

	iErr = FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Skeletons", &gDataSpec);
	if (iErr)
		DoFatalAlert("Cannot locate the Nanosaur Data folder.");



			/* LOAD QD3D & QT */
			
	QD3D_Boot();


			/* INIT PREFERENCES */

	LoadPrefs();

	SetFullscreenMode(true);
	SetProModeSettings(gGamePrefs.extreme);



			/* INIT SOME OF MY STUFF */

	InitSkeletonManager();
	InitSoundTools();
	Init3DMFManager();
	InitObjectManager();
	InitSpriteManager();
}


/***************** INIT LEVEL ************************/

static void InitLevel(void)
{
QD3DSetupInputType		viewDef;
TQ3ColorRGB		c1 = { 1.0, 1, 1 };
TQ3ColorRGB		c2 = { 1, .9, .6 };

	PlaySong(0,true);

			/* MAKE VIEW */

	QD3D_NewViewDef(&viewDef);
	
#if TWO_MEG_VERSION
	viewDef.view.paneClip.left 		+= 144;  
	viewDef.view.paneClip.right 	+= 32;  
	viewDef.view.paneClip.top		+= 40;  
	viewDef.view.paneClip.bottom 	+= 140;  
	viewDef.lights.ambientBrightness = 0.3;
#else	
	viewDef.view.paneClip.left 		+= 118;  
	viewDef.view.paneClip.right 	+= 12;  
	viewDef.view.paneClip.top		+= 9;  
	viewDef.view.paneClip.bottom 	+= 110;  
	viewDef.lights.ambientBrightness = 0.2;
#endif	
	viewDef.camera.hither 			= HITHER_DISTANCE;
	viewDef.camera.yon 				= YON_DISTANCE;
	viewDef.camera.fov 				= 1.0;
	viewDef.lights.fillColor[0] 	= c1;
	viewDef.lights.fillColor[1] 	= c2;
	viewDef.lights.fillBrightness[0] = 1.2;
	viewDef.lights.fillBrightness[1] = .4;

	viewDef.view.keepBackdropAspectRatio = gGamePrefs.force4x3;

	if (!gGamePrefs.whiteSky)
	{
		viewDef.view.clearColor.r = 0;
		viewDef.view.clearColor.g = 0;
		viewDef.view.clearColor.b = 0;
	}
	else
	{
		viewDef.view.clearColor.r = .95;
		viewDef.view.clearColor.g = .95;
		viewDef.view.clearColor.b = .75;
	}
	
	QD3D_SetupWindow(&viewDef, &gGameViewInfoPtr);
	QD3D_DrawScene(gGameViewInfoPtr, nil);


				/* INIT FLAGS */

	gGameOverFlag = false;
	gPlayerGotKilledFlag = false;
	gWonGameFlag = false;
	gMyHealth = 1.0;

			/* LOAD ART */

	MakeShadowTexture();

	LoadLevelArt(LEVEL_NUM_0);

	QD3D_InitShards();	
	InitWeaponManager();
	InitItemsManager();
	InitMyInventory();	
	InitInfobar();

		
		/* INIT THE PLAYER */
			
	InitMyGuy();
	InitEnemyManager();	
					
			/* INIT CAMERA */
			
	MakeCameraEvent();
	
			/* PREP THE TERRAIN */

	InitTerrainManager();
	PrimeInitialTerrain();
	InitTimePortals();
	
	StartAmbientEffect();
}


/**************** CLEANUP LEVEL **********************/

static void CleanupLevel(void)
{
	StopAllEffectChannels();
	DeleteAllObjects();
	FreeAllSkeletonFiles(-1);
	DisposeTerrain();
	DisposeSpriteGroup(0);
	QD3D_DisposeShards();
	DeleteAll3DMFGroups();
	QD3D_DisposeWindowSetup(&gGameViewInfoPtr);
}


/**************** PLAY LEVEL ************************/

static bool PlayLevel(void)
{
	static enum { LevelSetup, LevelRun, LevelPause, LevelFade, LevelMovie, LevelScores } state;
	static float killDelay;
	static FSSpec spec;
	float fps = gFramesPerSecondFrac;
	if (state == LevelSetup)
	{
	killDelay = KILL_DELAY;

			/* INIT LEVEL */
			
	InitLevel();

	gGameOverFlag = false;

	MakeFadeEvent(true);
	state = LevelRun;
	}
		

		/******************/
		/* MAIN GAME LOOP */
		/******************/
	
	if (state == LevelRun)
	{
		fps = gFramesPerSecondFrac;
		
		UpdateInput();


				/* MOVE OBJECTS */
				
		CalcPlayerKeyControls();
		MoveObjects();
		QD3D_MoveShards();

				/* SPECIFIC MAINTENANCE */
				
		UpdateLavaTextureAnimation();
		UpdateWaterTextureAnimation();
		DecAsteroidTimer();
	
	
				/* DRAW OBJECTS & TERRAIN */
					
		DoMyTerrainUpdate();
		UpdateInfobar();
		QD3D_DrawScene(gGameViewInfoPtr,DrawTerrain);

				
			/* SEE IF PAUSE GAME */

		if (GetNewNeedState(kNeed_UIPause)						// see if pause/abort
			|| IsCmdQPressed())
		{
			state = LevelPause;
			return false;
		}


			/* CHECK CHEAT KEYS */
			
		if (GetSDLKeyState(SDL_SCANCODE_F15) || GetSDLKeyState(SDL_SCANCODE_F12))
		{
			if (GetNewSDLKeyState(SDL_SCANCODE_F1))				// get health
				GetHealth(1);
			else
			if (GetNewSDLKeyState(SDL_SCANCODE_F2))				// get shield
				StartMyShield(gPlayerObj);
			else
			if (GetNewSDLKeyState(SDL_SCANCODE_F3))				// get full weaponry
				GetCheatWeapons();
			else
			if (GetNewSDLKeyState(SDL_SCANCODE_F4))				// get all eggs
				GetAllEggsCheat();
			else
			if (GetNewSDLKeyState(SDL_SCANCODE_F5))				// get fuel
			{
				gFuel = MAX_FUEL_CAPACITY;
				gInfobarUpdateBits |= UPDATE_FUEL;
			}
				
		}

			/* SEE IF GAME ENDED */				
		
		if (gGameOverFlag)
		{
			state = LevelFade;
			return false;
		}
			
			
			/* SEE IF GOT KILLED */
				
		if (gPlayerGotKilledFlag)				// if got killed, then hang around for a few seconds before resetting player
		{
			killDelay -= fps;					
			if (killDelay < 0.0f)				// see if time to reset player
			{
				ResetPlayer();					// reset player
				killDelay = KILL_DELAY;			// reset kill timer for next death
			}
		}
		return false;
	}
	if (state == LevelPause)
	{
		if (DoPaused()) state = gGameOverFlag ? LevelFade : LevelRun;
		return false;
	}

	if (state == LevelFade)
	{
		if (!Render_FreezeFrameFadeOut()) return false;
		CleanupLevel();
		FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID,
			gWonGameFlag ? ":Movies:Win.mov" : ":Movies:Lose.mov", &spec);
		state = LevelMovie;
	}
	if (state == LevelMovie)
	{
		if (!PlayAMovie(&spec)) return false;
		if (!gSongPlayingFlag) PlaySong(1, true);
		state = LevelScores;
	}
	if (!ShowHighScoresScreen(gScore)) return false;
	state = LevelSetup;
	return true;
}



/************************************************************/
/******************** PROGRAM MAIN ENTRY  *******************/
/************************************************************/


void GameMain(void)
{
	static enum { GameBoot, GameCharity, GameLogo, GameTitle, GameMenu, GameLevel } state;
	switch (state)
	{
		case GameBoot:
		{
			ToolBoxInit();
			InitInput();
			unsigned long seed;
			GetDateTime(&seed);
			SetMyRandomSeed(seed);
			state = GameCharity;
			break;
		}
		case GameCharity:
			if (ShowCharity())
			{
				LoadSoundBank();
				state = GameLogo;
			}
			break;
		case GameLogo:
			if (DoPangeaLogo()) state = GameTitle;
			break;
		case GameTitle:
			if (DoTitleScreen()) state = GameMenu;
			break;
		case GameMenu:
			if (DoMainMenu()) state = GameLevel;
			break;
		case GameLevel:
			if (PlayLevel()) state = GameLogo;
			break;
	}
}



