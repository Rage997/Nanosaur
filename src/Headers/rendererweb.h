#pragma once

enum
{
	kWebMeshGeometry = 1 << 0,	// Positions and normals (skinning/shards).
	kWebMeshAttributes = 1 << 1,	// UVs and vertex colors.
	kWebMeshTopology = 1 << 2,	// Triangle indices (terrain recycling).
	kWebMeshAll = kWebMeshGeometry | kWebMeshAttributes | kWebMeshTopology,
};

#ifdef __EMSCRIPTEN__
// Fixed-function state names remain internal to Renderer.c's native state cache.
#define GL_VERTEX_ARRAY 0x8074
#define GL_NORMAL_ARRAY 0x8075
#define GL_COLOR_ARRAY 0x8076
#define GL_TEXTURE_COORD_ARRAY 0x8078
#define GL_NORMALIZE 0x0BA1
#define GL_ALPHA_TEST 0x0BC0
#define GL_COLOR_MATERIAL 0x0B57
#define GL_LIGHTING 0x0B50
#define GL_FOG 0x0B60

void RenderWeb_Init(void);
void RenderWeb_BeginFrame(void);
void RenderWeb_Shutdown(void);
void RenderWeb_DrawMesh(const TQ3TriMeshData*, const TQ3Matrix4x4*, const RenderModifiers*);
void RenderWeb_DrawQuad(const TQ3Point2D*, const TQ3ColorRGBA*, bool textured);
void RenderWeb_InvalidateMesh(const TQ3TriMeshData*, unsigned mask);
void RenderWeb_ReleaseMesh(const TQ3TriMeshData*);
void RenderWeb_ClearMeshes(void);
void RenderWeb_UploadTexture(bool allocate, GLenum internalFormat, int x, int y,
    int width, int height, GLenum format, GLenum type, const void* pixels, int rowLength);
#else
#define RenderWeb_InvalidateMesh(mesh, mask) ((void)0)
#define RenderWeb_ReleaseMesh(mesh) ((void)0)
#define RenderWeb_ClearMeshes() ((void)0)
#define RenderWeb_BeginFrame() ((void)0)
#define RenderWeb_Shutdown() ((void)0)
#endif
