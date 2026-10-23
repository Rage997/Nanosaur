/****************************/
/*   		TITLE.C		    */
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


static void MoveTitleText(ObjNode *theNode);
static void MovePangeaLogoPart(ObjNode *theNode);
static void MoveTitleBG(ObjNode *theNode);


/****************************/
/*    CONSTANTS             */
/****************************/

// Source port addition.
// These values have been tweaked so that the intro screen looks good
// in widescreen without any gaps appearing in the background.
static const int	kBackgroundRepeat			= 3;		// was 2
static const float	kBackgroundScale			= 2.6f;		// was 2.5
static const float	kBackgroundLeftmostX		= -600.0f;	// was -480
static const float	kBackgroundLength			= 300.0f;


/*********************/
/*    VARIABLES      */
/*********************/


/********************** DO TITLE SCREEN *************************/

bool DoTitleScreen(void)
{
static enum { TitleSetup, TitleRun, TitleFade } state;
if (state == TitleSetup)
{
FSSpec spec;
QD3DSetupInputType		viewDef;
ObjNode			*dinoObj;
TQ3Point3D		cameraFrom = { 110, 90, 190.0 };



			/* MAKE VIEW */

	QD3D_NewViewDef(&viewDef);
	viewDef.camera.hither 			= 10;
	viewDef.camera.yon				= gGamePrefs.canDoFog? 500: 700;		// Source port change from 500 to look good in widescreen without fog
	viewDef.camera.fov 				= 1.0;
	viewDef.lights.fogHither		= .3;
	viewDef.styles.usePhong = false;
#if TWO_MEG_VERSION
	viewDef.view.paneClip.left 		+= 64;  
	viewDef.view.paneClip.right 	+= 64;  
	viewDef.view.paneClip.top		+= 50;  
	viewDef.view.paneClip.bottom 	+= 50;  
	viewDef.camera.from 			= cameraFrom2;
#else
	viewDef.camera.from 			= cameraFrom;
#endif
	
	if (!gGamePrefs.whiteSky)
	{
		viewDef.view.clearColor.r = 0;
		viewDef.view.clearColor.g = 0;
		viewDef.view.clearColor.b = 0;
	}
	else
	{
		viewDef.view.clearColor.r = 
		viewDef.view.clearColor.g = 
		viewDef.view.clearColor.b = 1;
	}
	
	
	
	QD3D_SetupWindow(&viewDef, &gGameViewInfoPtr);



			/* LOAD ART */

	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Title.3dmf", &spec);		// load other models
	LoadGrouped3DMF(&spec,MODEL_GROUP_TITLE);


	// Subdivide triangles in background mesh so per-vertex fog looks better on it
	// on systems that don't support per-pixel fog.
	Q3TriMeshData_SubdivideTriangles(gObjectGroupList[MODEL_GROUP_TITLE][TITLE_MObjType_Background].meshes[0]);
	Q3TriMeshData_SubdivideTriangles(gObjectGroupList[MODEL_GROUP_TITLE][TITLE_MObjType_Background].meshes[0]);


	LoadASkeleton(SKELETON_TYPE_REX);




	if (!gSongPlayingFlag)						// make sure music is going
		PlaySong(1,true);



			/* MAKE SKELETON 1 */
		
	gNewObjectDefinition.type 	= SKELETON_TYPE_REX;
	gNewObjectDefinition.animNum = 1;
	gNewObjectDefinition.scale = .5;
	gNewObjectDefinition.coord.y = 0;
	gNewObjectDefinition.coord.x = 10;
	gNewObjectDefinition.coord.z = 70;
	gNewObjectDefinition.slot = PLAYER_SLOT;
	gNewObjectDefinition.flags = STATUS_BIT_HIGHFILTER;
	gNewObjectDefinition.moveCall = nil;
	gNewObjectDefinition.rot = -PI/2;
	dinoObj = MakeNewSkeletonObject(&gNewObjectDefinition);	
	dinoObj->Skeleton->AnimSpeed = .8;
	UpdateObjectTransforms(dinoObj);



			/* MAKE TITLE NAME */
			
	gNewObjectDefinition.group = MODEL_GROUP_TITLE;	
	gNewObjectDefinition.type = TITLE_MObjType_GameName;	
	gNewObjectDefinition.coord.x = 60;
	gNewObjectDefinition.coord.y = 15;
	gNewObjectDefinition.coord.z = 100;
	gNewObjectDefinition.flags = STATUS_BIT_REFLECTIONMAP;
	gNewObjectDefinition.moveCall = MoveTitleText;
	gNewObjectDefinition.rot = .9;
	gNewObjectDefinition.scale = .4;
	MakeNewDisplayGroupObject(&gNewObjectDefinition);


			/* MAKE BACKGROUND */
			
	gNewObjectDefinition.group = MODEL_GROUP_TITLE;	
	gNewObjectDefinition.type = TITLE_MObjType_Background;	
	gNewObjectDefinition.coord.x = kBackgroundLeftmostX * kBackgroundScale;
	gNewObjectDefinition.coord.y = 0;
	gNewObjectDefinition.coord.z = -40;
	gNewObjectDefinition.flags = STATUS_BIT_DONTCULL;
	gNewObjectDefinition.moveCall = MoveTitleBG;
	gNewObjectDefinition.rot = 0;
	gNewObjectDefinition.scale = kBackgroundScale;

	for (int i = 0; i < kBackgroundRepeat; i++)
	{
		MakeNewDisplayGroupObject(&gNewObjectDefinition);
		gNewObjectDefinition.coord.x += kBackgroundLength * kBackgroundScale;
	}



			/*****************/
			/* ANIMATE TITLE */
			/*****************/
			
	gDisableAnimSounds = true;
			
	MakeFadeEvent(true);								// start fade-in		
			
	state = TitleRun;
}
	if (state == TitleRun)
	{
		MoveObjects();
		QD3D_DrawScene(gGameViewInfoPtr,DrawObjects);
		UpdateInput();
		if (!UserWantsOut()) return false;
		state = TitleFade;
	}
	
			/* CLEANUP */

	if (!Render_FreezeFrameFadeOut()) return false;

	DeleteAllObjects();
	FreeAllSkeletonFiles(-1);
	DeleteAll3DMFGroups();
	QD3D_DisposeWindowSetup(&gGameViewInfoPtr);		
	gDisableAnimSounds = false;
	state = TitleSetup;
	return true;
}



/**************** MOVE TITLE BG ********************/

static void MoveTitleBG(ObjNode *theNode)
{
	GetObjectInfo(theNode);

	gCoord.x -= gFramesPerSecondFrac * 65.0f;
	if (gCoord.x < (kBackgroundLeftmostX * kBackgroundScale))
	{
		gCoord.x += kBackgroundLength * kBackgroundScale * kBackgroundRepeat;
	}
	
	UpdateObject(theNode);

}


/******************* MOVE TITLE TEXT *************************/

static void MoveTitleText(ObjNode *theNode)
{
	theNode->SpecialF[0] += gFramesPerSecondFrac*1.8;
	
	theNode->Rot.y = .3 + (sin(theNode->SpecialF[0])+1.0) * .3;
	theNode->Rot.x = -.3;

	UpdateObjectTransforms(theNode);
}

/********************** DO PANGEA LOGO **********************************/

bool DoPangeaLogo(void)
{
static enum { LogoSetup, LogoRun, LogoFade } state;
if (state == LogoSetup)
{
ObjNode		*backObj;
TQ3Point3D			cameraFrom = { 0, 0, 70.0 };
QD3DSetupInputType		viewDef;
TQ3ColorRGB		c1 = { 1.0, 1, 1 };
TQ3ColorRGB		c2 = { 1, .9, .6 };
FSSpec			spec;

			/* MAKE VIEW */

	QD3D_NewViewDef(&viewDef);
	viewDef.camera.hither 			= 10;
	viewDef.camera.yon 				= 350;
	viewDef.camera.fov 				= 1.0;
	viewDef.camera.from				= cameraFrom;
	viewDef.lights.fillColor[0] 	= c1;
	viewDef.lights.fillColor[1] 	= c2;
	viewDef.view.clearColor.r = 
	viewDef.view.clearColor.g = 
	viewDef.view.clearColor.b = 0;
	viewDef.styles.usePhong = false;
	QD3D_SetupWindow(&viewDef, &gGameViewInfoPtr);


			/* LOAD ART */
			
	FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, ":Models:Title.3dmf", &spec);		// load other models
	LoadGrouped3DMF(&spec,MODEL_GROUP_TITLE);	




			/***************/
			/* MAKE MODELS */
			/***************/
				
			/* BACKGROUND */
			
	gNewObjectDefinition.group = MODEL_GROUP_TITLE;	
	gNewObjectDefinition.type = TITLE_MObjType_Pangea;
	gNewObjectDefinition.coord.x = 0;
	gNewObjectDefinition.coord.y = 0;
	gNewObjectDefinition.coord.z = -300;	
	gNewObjectDefinition.flags = STATUS_BIT_REFLECTIONMAP;
	gNewObjectDefinition.slot = 50;
	gNewObjectDefinition.moveCall = MovePangeaLogoPart;
	gNewObjectDefinition.rot = 0;
	gNewObjectDefinition.scale = .2;
	backObj = MakeNewDisplayGroupObject(&gNewObjectDefinition);
	backObj->Rot.y = -PI/2;
	backObj->Rot.x = -PI/2;
	
	
	MakeFadeEvent(true);
	
	
			/*************/
			/* MAIN LOOP */
			/*************/
			
	PlaySong(2,false);
	state = LogoRun;
}
	if (state == LogoRun)
	{
		UpdateInput();
		if (!UserWantsOut() && !gResetSong && gSongPlayingFlag)
		{
			MoveObjects();
			QD3D_DrawScene(gGameViewInfoPtr,DrawObjects);
			return false;
		}
		state = LogoFade;
	}

	if (!Render_FreezeFrameFadeOut()) return false;


			/***********/
			/* CLEANUP */
			/***********/
			
	DeleteAllObjects();
	DeleteAll3DMFGroups();
	QD3D_DisposeWindowSetup(&gGameViewInfoPtr);		
	KillSong();
	state = LogoSetup;
	return true;
}


/********************* MOVE PANGEA LOGO PART ***************************/

static void MovePangeaLogoPart(ObjNode *theNode)
{
	theNode->Coord.z += gFramesPerSecondFrac * 45;		
	theNode->Rot.y += gFramesPerSecondFrac * PI/9;		

	theNode->Rot.x = sin(theNode->SpecialF[0] += gFramesPerSecondFrac*1.5) * .3;
						
	UpdateObjectTransforms(theNode);
}


/*************** SLIDESHOW (source port refactor) **********************/

struct SlideshowEntry
{
	const char* imagePath;
	void (*postDrawCallback)(void);
};

static bool Slideshow(const struct SlideshowEntry* slides)
{
	static enum { SlidesSetup, SlideSetup, SlideRun, SlidesFade } state;
	static int index;
	static float age;
	static bool promptShown;
	if (state == SlidesSetup)
	{
		Render_InitState();
		Render_AllocBackdrop(GAME_VIEW_WIDTH, GAME_VIEW_HEIGHT);
		index = 0;
		state = SlideSetup;
	}
	if (state == SlideSetup)
	{
		FSSpec spec;
		const struct SlideshowEntry* slide = &slides[index];
		GAME_ASSERT(FSMakeFSSpec(gDataSpec.vRefNum, gDataSpec.parID, slide->imagePath, &spec) == noErr);
		PicHandle picture = GetPictureFromTGA(&spec);
		GAME_ASSERT(picture && *picture);
		DrawPicture(picture, &(*picture)->picFrame);
		DisposeHandle((Handle)picture);
		if (slide->postDrawCallback) slide->postDrawCallback();
		uint8_t* color = (uint8_t*)gBackdropPixels;
		Render_SetBackdropClearColor((TQ3ColorRGBA){color[1]/255.f, color[2]/255.f, color[3]/255.f, 1});
		age = 0;
		promptShown = false;
		UpdateInput();
		state = SlideRun;
	}
	if (state == SlideRun)
	{
		age += gFramesPerSecondFrac;
		float gamma = 100;
#if ALLOW_FADE
		if (index == 0) gamma = fminf(100, 100*age);
#endif
		Render_SetWindowGamma(gamma);
		if (!promptShown && age > 2)
		{
			MoveTo(494, 476);
			RGBBackColor2(0);
			RGBForeColor2(0xFFFFFF);
			DrawStringC(" Hit SPACE to continue ");
			promptShown = true;
		}
		UpdateInput();
		DoSoundMaintenance();
		Render_StartFrame();
		Render_DrawBackdrop(true);
		Render_EndFrame();
		SDL_GL_SwapWindow(gSDLWindow);
		if (gamma >= 100 && UserWantsOut())
		{
			index++;
			state = slides[index].imagePath ? SlideSetup : SlidesFade;
		}
		return false;
	}
	if (!Render_FreezeFrameFadeOut()) return false;
	Render_DisposeBackdrop();
	state = SlidesSetup;
	return true;
}


/*************** SHOW CHARITY **********************/
//
// OEM non-charity version
//

static void ShowCharity_SourcePortVersionOverlay(void)
{
	RGBBackColor2(PRO_MODE? 0xA0A0A0: 0xA5A5A5);
	RGBForeColor2(0x606060);
	MoveTo(4, 480-4);
	DrawStringC("Version " GAME_VERSION);
}

bool ShowCharity(void)
{
	const char* firstImage = PRO_MODE ? ":Images:Boot1Pro.tga" : ":Images:Boot1.tga";

	const struct SlideshowEntry slides[] = {
			{ firstImage, ShowCharity_SourcePortVersionOverlay },
			{ ":Images:Boot2.tga", NULL },
			{ NULL, NULL },
	};
	return Slideshow(slides);
}


/*************** SHOW HELP **********************/

static void ShowHelp_TechInfoOverlay(void)
{
	RGBForeColor2(0x404040);
	RGBBackColor2(0x9e9e9e);

	int LH = 14;

	int x = 4;
	int y = 480 - 4 - LH * 3;

	MoveTo(x, y); DrawStringC(GAME_FULL_NAME " " GAME_VERSION " / "); DrawStringC(SDL_GetCurrentVideoDriver());
	y += LH; MoveTo(x, y); DrawStringC("SDL "); DrawStringC(SDL_GetRevision());
	y += LH; MoveTo(x, y); DrawStringC("OpenGL "); DrawStringC((const char*) glGetString(GL_VERSION));
	y += LH; MoveTo(x, y); DrawStringC((const char*) glGetString(GL_RENDERER));
}

bool ShowHelp(void)
{
	const struct SlideshowEntry slides[] = {
			{ ":Images:Help1.tga", ShowHelp_TechInfoOverlay },
			{ NULL, NULL },
	};
	return Slideshow(slides);
}

