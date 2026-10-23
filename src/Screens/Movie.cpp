#include "game.h"
#include "PommeFiles.h"
#include "PommeGraphics.h"
#include "PommeVideo.h"
#include "Video/Cinepak.h"
#include <memory>

namespace
{
Pomme::Video::Movie LoadMovie(FSSpec* spec)
{
    short refNum;
    GAME_ASSERT_MESSAGE(FSpOpenDF(spec, fsRdPerm, &refNum) == noErr, "Cannot open movie");
    try
    {
        auto movie = Pomme::Video::ReadMoov(Pomme::Files::GetStream(refNum));
        FSClose(refNum);
        return movie;
    }
    catch (...)
    {
        FSClose(refNum);
        throw;
    }
}

struct MoviePlayback
{
    Pomme::Video::Movie movie;
    CinepakContext decoder;
    cmixer::SourceMixGuard audioGuard;
    float elapsed;
    bool fading = false;
    bool audioWasPlaying = false;

    explicit MoviePlayback(FSSpec* spec)
        : movie(LoadMovie(spec)), decoder(movie.width, movie.height),
          audioGuard(movie.audioStream), elapsed(0)
    {
        GAME_ASSERT(movie.width <= GAME_VIEW_WIDTH && movie.height <= GAME_VIEW_HEIGHT);
        GAME_ASSERT(movie.videoFrameRate > 0);
        elapsed = 1.0f / movie.videoFrameRate;
        Render_InitState();
        Render_AllocBackdrop(movie.width, movie.height);
        Render_SetBackdropClearColor({0, 0, 0, 1});
        movie.audioStream.Play();
    }
};
std::unique_ptr<MoviePlayback> playback;
}

void DisposeMovie(void)
{
    playback.reset();
}

void PauseMovie(bool pause)
{
    if (!playback) return;
    auto& source = playback->movie.audioStream;
    if (pause)
    {
        playback->audioWasPlaying = source.GetState() == cmixer::CM_STATE_PLAYING;
        if (playback->audioWasPlaying) source.Pause();
    }
    else if (playback->audioWasPlaying)
    {
        source.Play();
        playback->audioWasPlaying = false;
    }
}

bool PlayAMovie(FSSpec* spec)
{
    if (!playback)
    {
        gFadeOverlayOpacity = 0;
        KillSong();
        playback = std::make_unique<MoviePlayback>(spec);
    }
    auto& state = *playback;
    auto& movie = state.movie;
    UpdateInput();
    if (UserWantsOut()) state.fading = true;
    if (!state.fading)
    {
        state.elapsed += gFramesPerSecondFrac;
        const float interval = 1.0f / movie.videoFrameRate;
        bool decoded = false;
        while (state.elapsed >= interval && !movie.videoFrames.empty())
        {
            const auto& frame = movie.videoFrames.front();
            state.decoder.DecodeFrame(frame.data(), (int)frame.size());
            movie.videoFrames.pop();
            state.elapsed -= interval;
            decoded = true;
        }
        if (decoded)
        {
            UInt32* pixels = (UInt32*)GetPixBaseAddr(GetGWorldPixMap(Pomme::Graphics::GetScreenPort()));
            for (int y = 0; y < movie.height; y++)
            {
                const uint8_t* in = state.decoder.frame_data0 + state.decoder.frame_linesize0*y;
                uint8_t* out = (uint8_t*)(pixels + GAME_VIEW_WIDTH*y);
                for (int x = 0; x < movie.width; x++)
                {
                    *out++ = 0xFF;
                    *out++ = *in++;
                    *out++ = *in++;
                    *out++ = *in++;
                }
            }
            Rect damage = {0, 0, (SInt16)movie.height, (SInt16)movie.width};
            DamagePortRegion(&damage);
        }
        Render_StartFrame();
        Render_DrawBackdrop(true);
        Render_EndFrame();
        SDL_GL_SwapWindow(gSDLWindow);
        if (!movie.videoFrames.empty() || movie.audioStream.GetState() == cmixer::CM_STATE_PLAYING)
            return false;
        state.fading = true;
    }
    if (!Render_FreezeFrameFadeOut()) return false;
    Render_DisposeBackdrop();
    DisposeMovie();
    return true;
}
