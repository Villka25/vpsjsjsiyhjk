/*
   ███████╗██████╗  █████╗ ███╗   ██╗██╗   ██╗████████╗ █████╗ ██╗
   ██╔════╝██╔══██╗██╔══██╗████╗  ██║╚██╗ ██╔╝╚══██╔══╝██╔══██╗██║
   █████╗  ██████╔╝███████║██╔██╗ ██║ ╚████╔╝    ██║   ███████║██║
   ██╔══╝  ██╔══██╗██╔══██║██║╚██╗██║  ╚██╔╝     ██║   ██╔══██║██║
   ███████╗██████╔╝██║  ██║██║ ╚████║   ██║      ██║   ██║  ██║██║
   ╚══════╝╚═════╝ ╚═╝  ╚═╝╚═╝  ╚═══╝   ╚═╝      ╚═╝   ╚═╝  ╚═╝
   3D Engine on SDL2 + OpenGL 3.3, all-in-one file.
*/
#include <SDL2/SDL.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <vector>

// ================== OpenGL loader (вручную, без GLAD) ==================
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE0 0x84C0
#define GL_RGBA 0x1908
#define GL_RGB 0x1907
#define GL_UNSIGNED_BYTE 0x1401
#define GL_REPEAT 0x2901
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_LINEAR 0x2601
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW 0x88E4
#define GL_FLOAT 0x1406
#define GL_TRIANGLES 0x0004
#define GL_UNSIGNED_INT 0x1405
#define GL_DEPTH_TEST 0x0B71
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_FALSE 0
#define GL_TRUE 1
#define GL_NO_ERROR 0
#define GL_INVALID_ENUM 0x0500

typedef void (*GLACTIVETEXTUREPROC)(GLenum texture);
typedef void (*GLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (*GLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (*GLBINDTEXTUREPROC)(GLenum target, GLuint texture);
typedef void (*GLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (*GLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void (*GLCLEARPROC)(GLbitfield mask);
typedef void (*GLCLEARCOLORPROC)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void (*GLCOMPILESHADERPROC)(GLuint shader);
typedef GLuint (*GLCREATEPROGRAMPROC)();
typedef GLuint (*GLCREATESHADERPROC)(GLenum type);
typedef void (*GLDELETESHADERPROC)(GLuint shader);
typedef void (*GLDELETEPROGRAMPROC)(GLuint program);
typedef void (*GLDRAWARRAYSPROC)(GLenum mode, GLint first, GLsizei count);
typedef void (*GLDRAWELEMENTSPROC)(GLenum mode, GLsizei count, GLenum type, const void* indices);
typedef void (*GLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (*GLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void (*GLGENVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void (*GLGENERATEMIPMAPPROC)(GLenum target);
typedef GLenum (*GLGETERRORPROC)();
typedef void (*GLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (*GLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void (*GLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint* params);
typedef void (*GLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint* params);
typedef GLint (*GLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void (*GLLINKPROGRAMPROC)(GLuint program);
typedef void (*GLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar** string, const GLint* length);
typedef void (*GLTEXIMAGE2DPROC)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data);
typedef void (*GLTEXPARAMETERIPROC)(GLenum target, GLenum pname, GLint param);
typedef void (*GLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (*GLUNIFORM3FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void (*GLUNIFORM4FVPROC)(GLint location, GLsizei count, const GLfloat* value);
typedef void (*GLUNIFORMMATRIX4FVPROC)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (*GLUSEPROGRAMPROC)(GLuint program);
typedef void (*GLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);

#define GL_FUNC(ret, name, args) static name##PROC name = nullptr;
GL_FUNC(void, glActiveTexture, (GLenum texture))
GL_FUNC(void, glAttachShader, (GLuint program, GLuint shader))
GL_FUNC(void, glBindBuffer, (GLenum target, GLuint buffer))
GL_FUNC(void, glBindTexture, (GLenum target, GLuint texture))
GL_FUNC(void, glBindVertexArray, (GLuint array))
GL_FUNC(void, glBufferData, (GLenum target, GLsizeiptr size, const void* data, GLenum usage))
GL_FUNC(void, glClear, (GLbitfield mask))
GL_FUNC(void, glClearColor, (GLfloat r, GLfloat g, GLfloat b, GLfloat a))
GL_FUNC(void, glCompileShader, (GLuint shader))
GL_FUNC(GLuint, glCreateProgram, ())
GL_FUNC(GLuint, glCreateShader, (GLenum type))
GL_FUNC(void, glDeleteShader, (GLuint shader))
GL_FUNC(void, glDeleteProgram, (GLuint program))
GL_FUNC(void, glDrawArrays, (GLenum mode, GLint first, GLsizei count))
GL_FUNC(void, glDrawElements, (GLenum mode, GLsizei count, GLenum type, const void* indices))
GL_FUNC(void, glEnableVertexAttribArray, (GLuint index))
GL_FUNC(void, glGenBuffers, (GLsizei n, GLuint* buffers))
GL_FUNC(void, glGenVertexArrays, (GLsizei n, GLuint* arrays))
GL_FUNC(void, glGenerateMipmap, (GLenum target))
GL_FUNC(GLenum, glGetError, ())
GL_FUNC(void, glGetProgramInfoLog, (GLuint program, GLsizei maxLength, GLsizei* length, GLchar* infoLog))
GL_FUNC(void, glGetShaderInfoLog, (GLuint shader, GLsizei maxLength, GLsizei* length, GLchar* infoLog))
GL_FUNC(void, glGetProgramiv, (GLuint program, GLenum pname, GLint* params))
GL_FUNC(void, glGetShaderiv, (GLuint shader, GLenum pname, GLint* params))
GL_FUNC(GLint, glGetUniformLocation, (GLuint program, const GLchar* name))
GL_FUNC(void, glLinkProgram, (GLuint program))
GL_FUNC(void, glShaderSource, (GLuint shader, GLsizei count, const GLchar** string, const GLint* length))
GL_FUNC(void, glTexImage2D, (GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* data))
GL_FUNC(void, glTexParameteri, (GLenum target, GLenum pname, GLint param))
GL_FUNC(void, glUniform1i, (GLint location, GLint v0))
GL_FUNC(void, glUniform3fv, (GLint location, GLsizei count, const GLfloat* value))
GL_FUNC(void, glUniform4fv, (GLint location, GLsizei count, const GLfloat* value))
GL_FUNC(void, glUniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value))
GL_FUNC(void, glUseProgram, (GLuint program))
GL_FUNC(void, glVertexAttribPointer, (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer))
#undef GL_FUNC

void loadGLFunctions() {
#define LOAD(name) name = (name##PROC)SDL_GL_GetProcAddress(#name); if(!name) printf("Warning: %s not loaded\n", #name);
    LOAD(glActiveTexture)
    LOAD(glAttachShader)
    LOAD(glBindBuffer)
    LOAD(glBindTexture)
    LOAD(glBindVertexArray)
    LOAD(glBufferData)
    LOAD(glClear)
    LOAD(glClearColor)
    LOAD(glCompileShader)
    LOAD(glCreateProgram)
    LOAD(glCreateShader)
    LOAD(glDeleteShader)
    LOAD(glDeleteProgram)
    LOAD(glDrawArrays)
    LOAD(glDrawElements)
    LOAD(glEnableVertexAttribArray)
    LOAD(glGenBuffers)
    LOAD(glGenVertexArrays)
    LOAD(glGenerateMipmap)
    LOAD(glGetError)
    LOAD(glGetProgramInfoLog)
    LOAD(glGetShaderInfoLog)
    LOAD(glGetProgramiv)
    LOAD(glGetShaderiv)
    LOAD(glGetUniformLocation)
    LOAD(glLinkProgram)
    LOAD(glShaderSource)
    LOAD(glTexImage2D)
    LOAD(glTexParameteri)
    LOAD(glUniform1i)
    LOAD(glUniform3fv)
    LOAD(glUniform4fv)
    LOAD(glUniformMatrix4fv)
    LOAD(glUseProgram)
    LOAD(glVertexAttribPointer)
#undef LOAD
}

// ================== Математика (свой glm-like) ==================
struct vec3 { float x,y,z; vec3()=default; vec3(float a,float b,float c):x(a),y(b),z(c){} };
vec3 operator+(vec3 a,vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
vec3 operator-(vec3 a,vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
vec3 operator*(vec3 a,float s){return {a.x*s,a.y*s,a.z*s};}
vec3 normalize(vec3 v){float l=sqrtf(v.x*v.x+v.y*v.y+v.z*v.z);if(l>0.0001f)return {v.x/l,v.y/l,v.z/l};return v;}
vec3 cross(vec3 a,vec3 b){return {a.y*b.z-a.z*b.y, a.z*b.x-a.x*b.z, a.x*b.y-a.y*b.x};}
float dot(vec3 a,vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}

struct mat4 {
    float m[16];
    mat4(){memset(m,0,sizeof(m));}
    static mat4 identity(){mat4 r;for(int i=0;i<4;i++)r.m[i*4+i]=1;return r;}
};
mat4 perspective(float fov, float aspect, float near, float far){
    mat4 r; float tanHalfFov=tanf(fov/2);
    r.m[0]=1/(aspect*tanHalfFov); r.m[5]=1/tanHalfFov;
    r.m[10]=-(far+near)/(far-near); r.m[11]=-1;
    r.m[14]=-(2*far*near)/(far-near);
    return r;
}
mat4 lookAt(vec3 eye, vec3 center, vec3 up){
    vec3 f=normalize(center-eye); vec3 s=normalize(cross(f,up)); vec3 u=cross(s,f);
    mat4 r;
    r.m[0]=s.x; r.m[1]=u.x; r.m[2]=-f.x; r.m[3]=0;
    r.m[4]=s.y; r.m[5]=u.y; r.m[6]=-f.y; r.m[7]=0;
    r.m[8]=s.z; r.m[9]=u.z; r.m[10]=-f.z; r.m[11]=0;
    r.m[12]=-dot(s,eye); r.m[13]=-dot(u,eye); r.m[14]=dot(f,eye); r.m[15]=1;
    return r;
}
mat4 translate(mat4 m, vec3 v){
    mat4 r=m; r.m[12]+=v.x; r.m[13]+=v.y; r.m[14]+=v.z;
    return r;
}
mat4 scale(mat4 m, vec3 v){
    mat4 r=m; r.m[0]*=v.x; r.m[5]*=v.y; r.m[10]*=v.z;
    return r;
}
void mat4_to_gl(const mat4& m, float* out){
    for(int i=0;i<16;i++) out[i]=m.m[i];
}

// ================== Шейдеры (хардкод) ==================
const char* vertexSource = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in vec3 aNormal;

out vec2 TexCoord;
out vec3 Normal;
out vec3 FragPos;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main(){
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;
    gl_Position = projection * view * vec4(FragPos, 1.0);
    TexCoord = aTexCoord;
}
)";

const char* fragmentSource = R"(
#version 330 core
in vec2 TexCoord;
in vec3 Normal;
in vec3 FragPos;

out vec4 FragColor;

uniform sampler2D tex;
uniform vec3 lightPos;
uniform vec3 viewPos;
uniform vec3 lightColor;
uniform vec3 objectColor;
uniform float ambientStrength;
uniform float specularStrength;

void main(){
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = specularStrength * spec * lightColor;

    vec3 ambient = ambientStrength * lightColor;
    vec4 texColor = texture(tex, TexCoord);
    vec3 result = (ambient + diffuse + specular) * texColor.rgb * objectColor;
    FragColor = vec4(result, 1.0);
}
)";

// ================== Утилиты OpenGL ==================
GLuint compileShader(GLenum type, const char* src){
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success){
        char log[512]; glGetShaderInfoLog(shader,512,NULL,log);
        printf("Shader error: %s\n", log);
    }
    return shader;
}

GLuint createProgram(const char* vs, const char* fs){
    GLuint v = compileShader(GL_VERTEX_SHADER, vs);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, fs);
    GLuint p = glCreateProgram();
    glAttachShader(p, v); glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

// Процедурная текстура 64x64 (шахматная доска)
GLuint createCheckerTexture(){
    const int texSize = 64;
    unsigned char data[texSize][texSize][3];
    for(int y=0;y<texSize;y++){
        for(int x=0;x<texSize;x++){
            bool white = ((x/8)+(y/8))%2 == 0;
            data[y][x][0] = data[y][x][1] = data[y][x][2] = white?255:50;
        }
    }
    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, texSize, texSize, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

// Куб с нормалями и текстурными координатами
void createCube(GLuint& VAO, GLuint& VBO, GLuint& EBO){
    float vertices[] = {
        // позиция          texcoord   нормаль
        -0.5f,-0.5f,-0.5f,  0.0f,0.0f,  0.0f,0.0f,-1.0f,
         0.5f,-0.5f,-0.5f,  1.0f,0.0f,  0.0f,0.0f,-1.0f,
         0.5f, 0.5f,-0.5f,  1.0f,1.0f,  0.0f,0.0f,-1.0f,
        -0.5f, 0.5f,-0.5f,  0.0f,1.0f,  0.0f,0.0f,-1.0f,

        -0.5f,-0.5f, 0.5f,  0.0f,0.0f,  0.0f,0.0f,1.0f,
         0.5f,-0.5f, 0.5f,  1.0f,0.0f,  0.0f,0.0f,1.0f,
         0.5f, 0.5f, 0.5f,  1.0f,1.0f,  0.0f,0.0f,1.0f,
        -0.5f, 0.5f, 0.5f,  0.0f,1.0f,  0.0f,0.0f,1.0f,

        -0.5f, 0.5f, 0.5f,  1.0f,0.0f, -1.0f,0.0f,0.0f,
        -0.5f, 0.5f,-0.5f,  1.0f,1.0f, -1.0f,0.0f,0.0f,
        -0.5f,-0.5f,-0.5f,  0.0f,1.0f, -1.0f,0.0f,0.0f,
        -0.5f,-0.5f, 0.5f,  0.0f,0.0f, -1.0f,0.0f,0.0f,

         0.5f, 0.5f, 0.5f,  1.0f,0.0f,  1.0f,0.0f,0.0f,
         0.5f, 0.5f,-0.5f,  1.0f,1.0f,  1.0f,0.0f,0.0f,
         0.5f,-0.5f,-0.5f,  0.0f,1.0f,  1.0f,0.0f,0.0f,
         0.5f,-0.5f, 0.5f,  0.0f,0.0f,  1.0f,0.0f,0.0f,

        -0.5f,-0.5f,-0.5f,  0.0f,1.0f,  0.0f,-1.0f,0.0f,
         0.5f,-0.5f,-0.5f,  1.0f,1.0f,  0.0f,-1.0f,0.0f,
         0.5f,-0.5f, 0.5f,  1.0f,0.0f,  0.0f,-1.0f,0.0f,
        -0.5f,-0.5f, 0.5f,  0.0f,0.0f,  0.0f,-1.0f,0.0f,

        -0.5f, 0.5f,-0.5f,  0.0f,1.0f,  0.0f,1.0f,0.0f,
         0.5f, 0.5f,-0.5f,  1.0f,1.0f,  0.0f,1.0f,0.0f,
         0.5f, 0.5f, 0.5f,  1.0f,0.0f,  0.0f,1.0f,0.0f,
        -0.5f, 0.5f, 0.5f,  0.0f,0.0f,  0.0f,1.0f,0.0f
    };
    unsigned int indices[] = {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        8,9,10, 10,11,8,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,21,22, 22,23,20
    };
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 8*sizeof(float), (void*)(5*sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

// ================== Карта (1 - стена) ==================
int mapW=16, mapH=16;
int map[] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,1,
    1,0,0,1,0,0,0,0,0,0,0,0,1,0,0,1,
    1,0,0,1,0,0,0,0,0,0,0,0,1,0,0,1,
    1,0,0,1,0,0,0,0,0,0,0,0,1,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,1,0,0,0,0,0,0,0,0,1,0,0,1,
    1,0,0,1,0,0,0,0,0,0,0,0,1,0,0,1,
    1,0,0,1,0,0,0,0,0,0,0,0,1,0,0,1,
    1,0,0,1,1,1,1,0,0,1,1,1,1,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

// ================== Камера с коллизией ==================
struct Camera {
    vec3 pos, front, up;
    float yaw, pitch;
    float speed;
    Camera() : pos(2,2,4), front(0,0,-1), up(0,1,0), yaw(-90), pitch(0), speed(3.0f) {}
    void updateVectors(){
        vec3 newFront;
        newFront.x = cosf(yaw*0.0174533f) * cosf(pitch*0.0174533f);
        newFront.y = sinf(pitch*0.0174533f);
        newFront.z = sinf(yaw*0.0174533f) * cosf(pitch*0.0174533f);
        front = normalize(newFront);
    }
    void processMouse(float dx, float dy){
        yaw += dx*0.1f; pitch -= dy*0.1f;
        if(pitch>89) pitch=89; if(pitch<-89) pitch=-89;
        updateVectors();
    }
    void move(bool* keys, float dt){
        vec3 right = normalize(cross(front, up));
        vec3 moveDir{0,0,0};
        if(keys[SDL_SCANCODE_W]) moveDir = moveDir + front;
        if(keys[SDL_SCANCODE_S]) moveDir = moveDir - front;
        if(keys[SDL_SCANCODE_A]) moveDir = moveDir - right;
        if(keys[SDL_SCANCODE_D]) moveDir = moveDir + right;
        if(moveDir.x||moveDir.y||moveDir.z) moveDir = normalize(moveDir);
        float vel = speed * dt;
        // Простая проверка коллизии по карте (без скольжения по стенам, но норм)
        vec3 newPos = pos + moveDir * vel;
        int mx = int(newPos.x), my = int(newPos.z);
        if(mx>=0&&mx<mapW&&my>=0&&my<mapH&&map[my*mapW+mx]==0)
            pos = newPos;
    }
};

// ================== Основная функция ==================
int main(){
    SDL_Init(SDL_INIT_VIDEO);
    SDL_Window* win = SDL_CreateWindow("Ebanytai Engine", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768, SDL_WINDOW_OPENGL);
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    loadGLFunctions();
    glEnable(GL_DEPTH_TEST);

    GLuint program = createProgram(vertexSource, fragmentSource);
    GLuint tex = createCheckerTexture();
    GLuint cubeVAO, cubeVBO, cubeEBO;
    createCube(cubeVAO, cubeVBO, cubeEBO);

    Camera cam;
    bool running = true;
    SDL_Event e;
    float last = SDL_GetTicks()/1000.0f;

    while(running){
        float now = SDL_GetTicks()/1000.0f;
        float dt = now - last; last = now;
        if(dt>0.1f) dt=0.016f;

        while(SDL_PollEvent(&e)){
            if(e.type==SDL_QUIT||(e.type==SDL_KEYDOWN&&e.key.keysym.scancode==SDL_SCANCODE_ESCAPE))
                running = false;
            if(e.type==SDL_MOUSEMOTION)
                cam.processMouse(e.motion.xrel, e.motion.yrel);
        }
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        cam.move((bool*)keys, dt);

        glClearColor(0.2f,0.2f,0.2f,1);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

        glUseProgram(program);
        // Установка uniform'ов
        mat4 proj = perspective(70.0f*0.0174533f, 1024.0f/768, 0.1f, 100);
        mat4 view = lookAt(cam.pos, cam.pos+cam.front, cam.up);
        float projMat[16], viewMat[16];
        mat4_to_gl(proj, projMat); mat4_to_gl(view, viewMat);
        glUniformMatrix4fv(glGetUniformLocation(program,"projection"),1,GL_FALSE,projMat);
        glUniformMatrix4fv(glGetUniformLocation(program,"view"),1,GL_FALSE,viewMat);
        glUniform3fv(glGetUniformLocation(program,"lightPos"),1, &(vec3{4,3,4}.x));
        glUniform3fv(glGetUniformLocation(program,"viewPos"),1, &cam.pos.x);
        glUniform3fv(glGetUniformLocation(program,"lightColor"),1, &(vec3{1,1,1}.x));
        glUniform3fv(glGetUniformLocation(program,"objectColor"),1, &(vec3{0.8f,0.7f,0.6f}.x));
        glUniform1f(glGetUniformLocation(program,"ambientStrength"),0.2f);
        glUniform1f(glGetUniformLocation(program,"specularStrength"),0.5f);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(program,"tex"),0);

        // Рисуем кубы по карте
        for(int y=0; y<mapH; y++){
            for(int x=0; x<mapW; x++){
                if(map[y*mapW+x]==1){
                    mat4 model = translate(mat4::identity(), {(float)x,0,(float)y});
                    float modelMat[16]; mat4_to_gl(model, modelMat);
                    glUniformMatrix4fv(glGetUniformLocation(program,"model"),1,GL_FALSE,modelMat);
                    glBindVertexArray(cubeVAO);
                    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
                }
            }
        }
        SDL_GL_SwapWindow(win);
    }
    // cleanup опущен для краткости
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
