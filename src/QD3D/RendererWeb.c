#ifdef __EMSCRIPTEN__
#include "game.h"
#include "rendererweb.h"

extern TQ3Matrix4x4 gCameraWorldToViewMatrix, gCameraViewToFrustumMatrix;
extern TQ3Param2D gEnvMapUVs[];

#define GPU_MESH_CAPACITY 8192
#define TOMBSTONE ((const TQ3TriMeshData*)(uintptr_t)1)
typedef struct
{
    const TQ3TriMeshData* mesh;
    GLuint vao, buffers[5];
    size_t capacities[5];
    unsigned dirty;
} GPUMesh;
static GPUMesh meshes[GPU_MESH_CAPACITY];
static GLuint program, quadVAO, quadBuffers[2], environmentBuffer;
static size_t environmentCapacity;
static uint8_t* convertedPixels;
static size_t convertedCapacity;
static bool sceneDirty;
static struct
{
    GLint modelView, projection, normal, color, textured, lit, cutout, fog;
    GLint ambient, lightDirection, lightColor, lightCount, fogRange, fogColor;
} uniforms;

static const char* vertexShader =
    "#version 300 es\n"
    "precision highp float;\n"
    "layout(location=0) in vec3 position; layout(location=1) in vec3 normal;\n"
    "layout(location=2) in vec2 uv; layout(location=3) in vec4 vertexColor;\n"
    "uniform mat4 modelView, projection, normalMatrix;\n"
    "uniform vec4 color; uniform bool lit; uniform vec3 ambient;\n"
    "uniform vec3 lightDirection[4], lightColor[4]; uniform int lightCount;\n"
    "out vec2 texCoord; out vec4 shade; out float distanceToEye;\n"
    "void main(){ vec4 eye=modelView*vec4(position,1.0);\n"
    "gl_Position=projection*eye; distanceToEye=abs(eye.z); texCoord=uv;\n"
    "shade=color*vertexColor; if(lit){ vec3 n=normalize(mat3(normalMatrix)*normal);\n"
    "vec3 lighting=ambient; for(int i=0;i<4;i++){if(i<lightCount)\n"
    "lighting+=lightColor[i]*max(dot(n,lightDirection[i]),0.0);}\n"
    "shade.rgb=clamp(shade.rgb*lighting,0.0,1.0); }}\n";
static const char* fragmentShader =
    "#version 300 es\nprecision highp float;\n"
    "in vec2 texCoord; in vec4 shade; in float distanceToEye;\n"
    "uniform sampler2D image; uniform bool textured, cutout, fog;\n"
    "uniform vec2 fogRange; uniform vec3 fogColor; out vec4 outputColor;\n"
    "void main(){ vec4 c=shade; if(textured)c*=texture(image,texCoord);\n"
    "if(cutout && c.a<=0.4999)discard; if(fog){float f=clamp(\n"
    "(fogRange.y-distanceToEye)/(fogRange.y-fogRange.x),0.0,1.0);\n"
    "c.rgb=mix(fogColor,c.rgb,f);} outputColor=c;}\n";

static GLuint CompileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success)
    {
        char log[2048];
        glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        DoFatalAlert2("WebGL shader compilation failed", log);
    }
    return shader;
}

void RenderWeb_Init(void)
{
    if (program) return;
    GLuint vertex = CompileShader(GL_VERTEX_SHADER, vertexShader);
    GLuint fragment = CompileShader(GL_FRAGMENT_SHADER, fragmentShader);
    program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success)
    {
        char log[2048];
        glGetProgramInfoLog(program, sizeof(log), NULL, log);
        DoFatalAlert2("WebGL shader linking failed", log);
    }
#define UNIFORM(field, name) uniforms.field = glGetUniformLocation(program, name)
    UNIFORM(modelView, "modelView"); UNIFORM(projection, "projection");
    UNIFORM(normal, "normalMatrix"); UNIFORM(color, "color");
    UNIFORM(textured, "textured"); UNIFORM(lit, "lit");
    UNIFORM(cutout, "cutout"); UNIFORM(fog, "fog");
    UNIFORM(ambient, "ambient"); UNIFORM(lightDirection, "lightDirection");
    UNIFORM(lightColor, "lightColor"); UNIFORM(lightCount, "lightCount");
    UNIFORM(fogRange, "fogRange"); UNIFORM(fogColor, "fogColor");
#undef UNIFORM
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "image"), 0);
    const float vertices[] = {-1,-1,0,1, 1,-1,1,1, -1,1,0,0, 1,1,1,0};
    const uint8_t indices[] = {0,1,2,1,3,2};
    glGenVertexArrays(1, &quadVAO);
    glBindVertexArray(quadVAO);
    glGenBuffers(2, quadBuffers);
    glBindBuffer(GL_ARRAY_BUFFER, quadBuffers[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), 0);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 4*sizeof(float), (void*)(2*sizeof(float)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(2);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, quadBuffers[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glGenBuffers(1, &environmentBuffer);
    glBindVertexArray(0);
}

static GPUMesh* FindMesh(const TQ3TriMeshData* mesh, bool create)
{
    unsigned index = ((uintptr_t)mesh >> 4) % GPU_MESH_CAPACITY;
    GPUMesh* vacant = NULL;
    for (unsigned i = 0; i < GPU_MESH_CAPACITY; i++, index = (index+1)%GPU_MESH_CAPACITY)
    {
        GPUMesh* slot = &meshes[index];
        if (slot->mesh == mesh) return slot;
        if (slot->mesh == TOMBSTONE && !vacant) vacant = slot;
        if (!slot->mesh)
        {
            if (!vacant) vacant = slot;
            break;
        }
    }
    if (!create) return NULL;
    GAME_ASSERT_MESSAGE(vacant, "WebGL mesh cache exhausted");
    vacant->mesh = mesh;
    vacant->dirty = kWebMeshAll;
    glGenVertexArrays(1, &vacant->vao);
    glGenBuffers(5, vacant->buffers);
    return vacant;
}

void RenderWeb_InvalidateMesh(const TQ3TriMeshData* mesh, unsigned mask)
{
    GPUMesh* gpu = FindMesh(mesh, false);
    if (gpu) gpu->dirty |= mask;
}
void RenderWeb_ReleaseMesh(const TQ3TriMeshData* mesh)
{
    GPUMesh* gpu = FindMesh(mesh, false);
    if (!gpu) return;
    glDeleteVertexArrays(1, &gpu->vao);
    glDeleteBuffers(5, gpu->buffers);
    *gpu = (GPUMesh){.mesh = TOMBSTONE};
}
void RenderWeb_ClearMeshes(void)
{
    for (unsigned i = 0; i < GPU_MESH_CAPACITY; i++)
        if (meshes[i].mesh && meshes[i].mesh != TOMBSTONE)
            RenderWeb_ReleaseMesh(meshes[i].mesh);
    SDL_memset(meshes, 0, sizeof(meshes));
}

void RenderWeb_BeginFrame(void)
{
    sceneDirty = true;
}

void RenderWeb_Shutdown(void)
{
    RenderWeb_ClearMeshes();
    glDeleteProgram(program);
    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(2, quadBuffers);
    glDeleteBuffers(1, &environmentBuffer);
    SDL_free(convertedPixels);
    convertedPixels = NULL;
    convertedCapacity = environmentCapacity = 0;
    program = quadVAO = environmentBuffer = 0;
}

static void UploadBuffer(GLenum target, size_t* capacity, size_t size, const void* data)
{
    if (size > *capacity)
    {
        glBufferData(target, size, NULL, GL_DYNAMIC_DRAW);
        *capacity = size;
    }
    glBufferSubData(target, 0, size, data);
}

static void Attribute(GLuint buffer, size_t* capacity, GLuint attribute, int components,
                      int count, const void* data, bool upload)
{
    if (!data)
    {
        glDisableVertexAttribArray(attribute);
        return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    if (upload) UploadBuffer(GL_ARRAY_BUFFER, capacity, count*components*sizeof(float), data);
    glVertexAttribPointer(attribute, components, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(attribute);
}

void RenderWeb_DrawMesh(const TQ3TriMeshData* mesh, const TQ3Matrix4x4* transform, const RenderModifiers* mods)
{
    GPUMesh* gpu = FindMesh(mesh, true);
    glUseProgram(program);
    glBindVertexArray(gpu->vao);
    Attribute(gpu->buffers[0], &gpu->capacities[0], 0, 3, mesh->numPoints, mesh->points, gpu->dirty & kWebMeshGeometry);
    Attribute(gpu->buffers[1], &gpu->capacities[1], 1, 3, mesh->numPoints, mesh->hasVertexNormals ? mesh->vertexNormals : NULL, gpu->dirty & kWebMeshGeometry);
    Attribute(gpu->buffers[2], &gpu->capacities[2], 2, 2, mesh->numPoints, mesh->vertexUVs, gpu->dirty & kWebMeshAttributes);
    Attribute(gpu->buffers[3], &gpu->capacities[3], 3, 4, mesh->numPoints, mesh->hasVertexColors ? mesh->vertexColors : NULL, gpu->dirty & kWebMeshAttributes);
    glVertexAttrib3f(1, 0, 0, 1);
    glVertexAttrib4f(3, 1, 1, 1, 1);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpu->buffers[4]);
    if (gpu->dirty & kWebMeshTopology)
        UploadBuffer(GL_ELEMENT_ARRAY_BUFFER, &gpu->capacities[4], mesh->numTriangles*sizeof(*mesh->triangles), mesh->triangles);
    gpu->dirty = 0;
    if (mods->statusBits & STATUS_BIT_REFLECTIONMAP)
    {
        EnvironmentMapTriMesh(mesh, transform);
        size_t size = mesh->numPoints*sizeof(TQ3Param2D);
        glBindBuffer(GL_ARRAY_BUFFER, environmentBuffer);
        if (size > environmentCapacity)
        {
            environmentCapacity = size;
            glBufferData(GL_ARRAY_BUFFER, size, NULL, GL_DYNAMIC_DRAW);
        }
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, gEnvMapUVs);
        glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(2);
    }
    TQ3Matrix4x4 modelView = gCameraWorldToViewMatrix;
    if (transform) Q3Matrix4x4_Multiply(transform, &gCameraWorldToViewMatrix, &modelView);
    glUniformMatrix4fv(uniforms.modelView, 1, GL_FALSE, &modelView.value[0][0]);
    bool lit = !(mods->statusBits & STATUS_BIT_NULLSHADER);
    if (lit)
    {
        TQ3Matrix4x4 normal;
        Q3Matrix4x4_Invert(&modelView, &normal);
        Q3Matrix4x4_Transpose(&normal, &normal);
        glUniformMatrix4fv(uniforms.normal, 1, GL_FALSE, &normal.value[0][0]);
    }
    glUniform4f(uniforms.color, mesh->diffuseColor.r*mods->diffuseColor.r,
        mesh->diffuseColor.g*mods->diffuseColor.g, mesh->diffuseColor.b*mods->diffuseColor.b,
        mesh->diffuseColor.a*mods->diffuseColor.a);
    bool transparent = mesh->texturingMode == kQ3TexturingModeAlphaBlend || mesh->diffuseColor.a < .999f || mods->diffuseColor.a < .999f;
    bool textured = mesh->texturingMode != kQ3TexturingModeOff;
    glUniform1i(uniforms.lit, lit);
    glUniform1i(uniforms.textured, textured);
    glUniform1i(uniforms.cutout, !transparent);
    if (textured) Render_BindTexture(mesh->glTextureName);
    if (sceneDirty)
    {
        glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, &gCameraViewToFrustumMatrix.value[0][0]);
        const QD3DLightDefType* lights = &gGameViewInfoPtr->lights;
        glUniform3f(uniforms.ambient, lights->ambientBrightness*lights->ambientColor.r,
            lights->ambientBrightness*lights->ambientColor.g, lights->ambientBrightness*lights->ambientColor.b);
        GLfloat directions[12] = {0}, colors[12] = {0};
        GAME_ASSERT(lights->numFillLights <= 4);
        for (int i = 0; i < lights->numFillLights; i++)
        {
            TQ3Vector3D direction;
            Q3Vector3D_Transform(&lights->fillDirection[i], &gCameraWorldToViewMatrix, &direction);
            Q3Vector3D_Normalize(&direction, &direction);
            directions[i*3] = -direction.x; directions[i*3+1] = -direction.y; directions[i*3+2] = -direction.z;
            colors[i*3] = lights->fillColor[i].r*lights->fillBrightness[i];
            colors[i*3+1] = lights->fillColor[i].g*lights->fillBrightness[i];
            colors[i*3+2] = lights->fillColor[i].b*lights->fillBrightness[i];
        }
        glUniform1i(uniforms.lightCount, lights->numFillLights);
        glUniform3fv(uniforms.lightDirection, 4, directions);
        glUniform3fv(uniforms.lightColor, 4, colors);
        glUniform1i(uniforms.fog, lights->useFog && gGamePrefs.canDoFog);
        float hither = gGameViewInfoPtr->hither, extent = gGameViewInfoPtr->yon-hither;
        glUniform2f(uniforms.fogRange, hither+lights->fogHither*extent, hither+lights->fogYon*extent);
        glUniform3fv(uniforms.fogColor, 1, &gGameViewInfoPtr->clearColor.r);
        sceneDirty = false;
    }
    if (transparent) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    glDepthMask(!transparent && !(mods->statusBits & STATUS_BIT_NOZWRITE));
    bool doubleSided = mods->statusBits & STATUS_BIT_KEEPBACKFACES;
    if (doubleSided && !transparent) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
    glCullFace(doubleSided && transparent ? GL_FRONT : GL_BACK);
    glDrawElements(GL_TRIANGLES, mesh->numTriangles*3, GL_UNSIGNED_INT, 0);
    if (doubleSided && transparent)
    {
        glCullFace(GL_BACK);
        glDrawElements(GL_TRIANGLES, mesh->numTriangles*3, GL_UNSIGNED_INT, 0);
    }
    CHECK_GL_ERROR();
}

void RenderWeb_DrawQuad(const TQ3Point2D* points, const TQ3ColorRGBA* color, bool textured)
{
    TQ3Matrix4x4 matrix, identity;
    sceneDirty = true;   // Quads overwrite projection and fog uniforms.
    Q3Matrix4x4_SetIdentity(&matrix);
    Q3Matrix4x4_SetIdentity(&identity);
    matrix.value[0][0] = (points[1].x-points[0].x)*.5f;
    matrix.value[1][1] = (points[2].y-points[0].y)*.5f;
    matrix.value[3][0] = (points[1].x+points[0].x)*.5f;
    matrix.value[3][1] = (points[2].y+points[0].y)*.5f;
    glUseProgram(program);
    glUniformMatrix4fv(uniforms.modelView, 1, GL_FALSE, &matrix.value[0][0]);
    glUniformMatrix4fv(uniforms.projection, 1, GL_FALSE, &identity.value[0][0]);
    glUniform4fv(uniforms.color, 1, &color->r);
    glUniform1i(uniforms.textured, textured);
    glUniform1i(uniforms.lit, false);
    glUniform1i(uniforms.fog, false);
    glUniform1i(uniforms.cutout, false);
    glBindVertexArray(quadVAO);
    glVertexAttrib4f(3, 1, 1, 1, 1);
    glDisable(GL_CULL_FACE);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_BYTE, 0);
}

void RenderWeb_UploadTexture(bool allocate, GLenum internalFormat, int x, int y,
    int width, int height, GLenum format, GLenum type, const void* pixels, int rowLength)
{
    if (!rowLength) rowLength = width;
    if (format == GL_BGRA && pixels)
    {
        size_t size = (size_t)width*height*4;
        if (size > convertedCapacity)
        {
            void* next = SDL_realloc(convertedPixels, size);
            GAME_ASSERT(next);
            convertedPixels = next;
            convertedCapacity = size;
        }
        for (int row = 0; row < height; row++)
            for (int col = 0; col < width; col++)
            {
                size_t index = row*rowLength+col;
                uint8_t* out = convertedPixels+4*(row*width+col);
                if (type == GL_UNSIGNED_SHORT_1_5_5_5_REV)
                {
                    uint16_t pixel = ((const uint16_t*)pixels)[index];
                    out[0] = ((pixel>>10)&31)*255/31;
                    out[1] = ((pixel>>5)&31)*255/31;
                    out[2] = (pixel&31)*255/31;
                    out[3] = pixel&0x8000 ? 255 : 0;
                }
                else
                {
                    const uint8_t* in = (const uint8_t*)pixels+index*4;
                    if (type == GL_UNSIGNED_INT_8_8_8_8)
                    { out[0]=in[1]; out[1]=in[2]; out[2]=in[3]; out[3]=in[0]; }
                    else
                    { out[0]=in[2]; out[1]=in[1]; out[2]=in[0]; out[3]=in[3]; }
                }
                if (internalFormat == GL_RGB) out[3] = 255;
            }
        pixels = convertedPixels;
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        rowLength = width;
    }
    if (format == GL_BGRA) { format = GL_RGBA; type = GL_UNSIGNED_BYTE; }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, rowLength);
    if (allocate) glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, type, pixels);
    else glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, format, type, pixels);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    CHECK_GL_ERROR();
}
#endif
