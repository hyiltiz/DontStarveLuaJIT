#include <Windows.h>
#include <dbghelp.h>
#include <vector>
#pragma comment(lib, "dbghelp.lib")

#pragma region OpenGL

typedef void(_stdcall* PFNGLBINDBUFFERARBPROC)(int target, unsigned int buffer);
PFNGLBINDBUFFERARBPROC glBindBuffer;

typedef void(_stdcall* PFNGLBUFFERDATAARBPROC)(int target, int size, const void* data, int usage);
PFNGLBUFFERDATAARBPROC glBufferData;

typedef void(_stdcall* PFNGLGENBUFFERSARBPROC)(int n, int* buffers);
PFNGLGENBUFFERSARBPROC glGenBuffers;

typedef void(_stdcall* PFNGLDELETEBUFFERSARBPROC)(int n, const int* buffers);
PFNGLDELETEBUFFERSARBPROC glDeleteBuffers;

typedef void (_stdcall* PFNGLDRAWARRAYSINSTANCEDPROC) (int mode, int first, int count, int primcount);
PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;

typedef void (_stdcall* PFNGLVERTEXATTRIBDIVISORPROC) (int index, int divisor);
PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor;

typedef void (_stdcall* PFNGLVERTEXATTRIBPOINTERARBPROC) (int index, int size, int type, bool normalized, int stride, const void* pointer);
PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointer;

typedef void (_stdcall* PFNGLUSEPROGRAMPROC) (int program);
PFNGLUSEPROGRAMPROC glUseProgram;

typedef int(_stdcall* PFNGLCREATESHADERPROC) (int type);
PFNGLCREATESHADERPROC glCreateShader;

typedef void (_stdcall* PFNGLSHADERSOURCEPROC) (int shader, int count, const char* const* strings, const int* length);
PFNGLSHADERSOURCEPROC glShaderSource;

typedef void(_stdcall* PFNGLCOMPILESHADERPROC) (int id);
PFNGLCOMPILESHADERPROC glCompileShader;

typedef void(_stdcall* PFNGLATTACHSHADERPROC) (int program, int shader);
PFNGLATTACHSHADERPROC glAttachShader;

typedef void(_stdcall* PFNGLLINKPROGRAMPROC) (int id);
PFNGLLINKPROGRAMPROC glLinkProgram;

typedef int(_stdcall* PFNGLCREATEPROGRAMPROC) ();
PFNGLCREATEPROGRAMPROC glCreateProgram;

typedef void (_stdcall * PFNGLBUFFERSUBDATAPROC) (int target, int offset, int size, const void* data);
PFNGLBUFFERSUBDATAPROC glBufferSubData;


typedef void (_stdcall * PFNGLDRAWARRAYSPROC) (int, int, int);
PFNGLDRAWARRAYSPROC glDrawArrays;

const char* instancedVertexShader = "\n\
uniform mat4 MatrixP;\n\
uniform mat4 MatrixV;\n\
attribute mat4 MatrixW;\n\
attribute vec3 POSITION;\n\
attribute vec3 TEXCOORD0;\n\
varying vec3 PS_TEXCOORD;\n\
varying vec3 PS_POS;\n\
#if defined( FADE_OUT )\n\
uniform mat4 STATIC_WORLD_MATRIX;\n\
varying vec2 FADE_UV;\n\
#endif\n\
\n\
void main()\n\
{\n\
	mat4 mtxPVW = MatrixP * MatrixV * MatrixW;\n\
	gl_Position = mtxPVW * vec4(POSITION.xyz, 1.0);\n\
\n\
	vec4 world_pos = MatrixW * vec4(POSITION.xyz, 1.0);\n\
\n\
	PS_TEXCOORD = TEXCOORD0;\n\
	PS_POS = world_pos.xyz;\n\
\n\
#if defined( FADE_OUT )\n\
	vec4 static_world_pos = STATIC_WORLD_MATRIX * vec4(POSITION.xyz, 1.0);\n\
	vec3 forward = normalize(vec3(MatrixV[2][0], 0.0, MatrixV[2][2]));\n\
	float d = dot(static_world_pos.xyz, forward);\n\
	vec3 pos = static_world_pos.xyz + (forward * -d);\n\
	vec3 left = cross(forward, vec3(0.0, 1.0, 0.0));\n\
\n\
	FADE_UV = vec2(dot(pos, left) / 4.0, static_world_pos.y / 8.0);\n\
#endif\n\
}\n";

static const char* instancedFragmentShader = " \n\
#if defined( GL_ES ) \n\
precision mediump float; \n\
#endif \n\
 \n\
uniform sampler2D SAMPLER[4]; \n\
 \n\
#ifndef LIGHTING_H \n\
#define LIGHTING_H \n\
 \n\
// Lighting \n\
varying vec3 PS_POS; \n\
uniform vec3 AMBIENT; \n\
 \n\
// xy = min, zw = max \n\
uniform vec4 LIGHTMAP_WORLD_EXTENTS; \n\
 \n\
#define LIGHTMAP_TEXTURE SAMPLER[3] \n\
 \n\
#ifndef LIGHTMAP_TEXTURE \n\
#error If you use lighting, you must #define the sampler that the lightmap belongs to \n\
#endif \n\
 \n\
vec3 CalculateLightingContribution() { \n\
	vec2 uv = (PS_POS.xz - LIGHTMAP_WORLD_EXTENTS.xy) * LIGHTMAP_WORLD_EXTENTS.zw; \n\
 \n\
	vec3 colour = texture2D(LIGHTMAP_TEXTURE, uv.xy).rgb + AMBIENT.rgb; \n\
 \n\
	return clamp(colour.rgb, vec3(0, 0, 0), vec3(1, 1, 1)); \n\
} \n\
 \n\
vec3 CalculateLightingContribution(vec3 normal) { \n\
	return vec3(1, 1, 1); \n\
} \n\
 \n\
#endif //LIGHTING.h \n\
 \n\
 \n\
varying vec3 PS_TEXCOORD; \n\
 \n\
uniform vec4 TINT_ADD; \n\
uniform vec4 TINT_MULT; \n\
uniform vec2 PARAMS; \n\
 \n\
#define ALPHA_TEST PARAMS.x \n\
#define LIGHT_OVERRIDE PARAMS.y \n\
 \n\
#if defined( FADE_OUT ) \n\
uniform vec3 EROSION_PARAMS; \n\
varying vec2 FADE_UV; \n\
 \n\
#define ERODE_SAMPLER SAMPLER[2] \n\
#define EROSION_MIN EROSION_PARAMS.x \n\
#define EROSION_RANGE EROSION_PARAMS.y \n\
#define EROSION_LERP EROSION_PARAMS.z \n\
#endif \n\
 \n\
void main() { \n\
	vec4 colour; \n\
	if (PS_TEXCOORD.z < 0.5) { \n\
		colour.rgba = texture2D(SAMPLER[0], PS_TEXCOORD.xy); \n\
	} else { \n\
		colour.rgba = texture2D(SAMPLER[1], PS_TEXCOORD.xy); \n\
	} \n\
 \n\
	if (colour.a >= ALPHA_TEST) { \n\
		gl_FragColor.rgba = colour.rgba; \n\
		gl_FragColor.rgba *= TINT_MULT.rgba; \n\
		gl_FragColor.rgb += vec3(TINT_ADD.rgb * colour.a); \n\
 \n\
#if defined( FADE_OUT ) \n\
		float height = texture2D(ERODE_SAMPLER, FADE_UV.xy).a; \n\
		float erode_val = clamp((height - EROSION_MIN) / EROSION_RANGE, 0.0, 1.0); \n\
		gl_FragColor.rgba = mix(gl_FragColor.rgba, gl_FragColor.rgba * erode_val, EROSION_LERP); \n\
#endif \n\
 \n\
		vec3 light = CalculateLightingContribution(); \n\
		gl_FragColor.rgb *= max(light.rgb, vec3(LIGHT_OVERRIDE, LIGHT_OVERRIDE, LIGHT_OVERRIDE)); \n\
	} else { \n\
		discard; \n\
	} \n\
} \n\
";
static bool ReplaceProgram = false;
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8

struct Matrix {
	float value[16];
};

std::vector<Matrix> instancedBufferData(1024 * 64);
size_t updatedInstancedBufferCount = 0;
size_t instancedBufferCount = 0;
int instancedBuffer = 0;
bool bufferSizeChanged = false;

void _stdcall Hook_SwapContext() {
	// finish a frame
	instancedBufferCount = 0;
	updatedInstancedBufferCount = 0;
}

void AddSpirteWorldMatrix(float* data) {
	if (instancedBufferCount > instancedBufferData.size()) {
		bufferSizeChanged = true;
		instancedBufferData.resize(instancedBufferData.size() * 2);
	}

	memcpy(instancedBufferData[instancedBufferCount].value, data, sizeof(Matrix));
}

void UpdateWorldMatrices() {
	if (updatedInstancedBufferCount != instancedBufferCount) {
		if (instancedBuffer == 0) {
			glGenBuffers(1, &instancedBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, instancedBuffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Matrix) * instancedBufferData.size(), NULL, GL_DYNAMIC_DRAW);
		} else {
			glBindBuffer(GL_ARRAY_BUFFER, instancedBuffer);
			if (bufferSizeChanged) {
				glBufferData(GL_ARRAY_BUFFER, sizeof(Matrix) * instancedBufferData.size(), &instancedBufferData[0], GL_DYNAMIC_DRAW);
				bufferSizeChanged = false;
			} else {
				glBufferSubData(GL_ARRAY_BUFFER, updatedInstancedBufferCount * sizeof(Matrix), (instancedBufferCount - updatedInstancedBufferCount) * sizeof(Matrix), &instancedBufferData[0]);
			}
		}

		updatedInstancedBufferCount = instancedBufferCount;
	}
}

void _stdcall Hook_glUseProgram(int program) {
	if (ReplaceProgram) {
		static int instancedProgram = 0;
		if (instancedProgram == 0) {
			instancedProgram = glCreateProgram();
			int vs = glCreateShader(GL_VERTEX_SHADER);
			glShaderSource(vs, 1, &instancedVertexShader, NULL);
			glCompileShader(vs);
			int fs = glCreateShader(GL_FRAGMENT_SHADER);
			glShaderSource(fs, 1, &instancedFragmentShader, NULL);
			glCompileShader(fs);
			glAttachShader(instancedProgram, vs);
			glAttachShader(instancedProgram, fs);
			glLinkProgram(instancedProgram);
		}

		glUseProgram(instancedProgram);
	} else {
		glUseProgram(program);
	}
}

struct Object {

};

template <class D, class T>
D ForceCast(T t) {
	union {
		D d;
		T t;
	} p;

	p.t = t;
	return p.d;
}

void DrawArraysSprite();

struct GLContext {
	typedef void (__thiscall *PFNPREPAREVERTEXBUFFER)(void*, int, Object*);
	static PFNPREPAREVERTEXBUFFER PrepareVertexBuffer;

	typedef void (__thiscall *PFNBINDVERTEXBUFFER)(void*);
	static PFNBINDVERTEXBUFFER BindVertexBuffer;

	typedef void* (__thiscall *PFNGETBUFFER)(void*, DWORD);
	static PFNGETBUFFER GetBuffer;
	
	typedef void (__thiscall *PFNFREE)(void*, int);
	static PFNFREE Free;


	void GetGLAddresses(HMODULE glModule) {
		glBindBuffer = (PFNGLBINDBUFFERARBPROC)GetProcAddress(glModule, "glBindBuffer");
		glBufferData = (PFNGLBUFFERDATAARBPROC)GetProcAddress(glModule, "glBufferData");
		glGenBuffers = (PFNGLGENBUFFERSARBPROC)GetProcAddress(glModule, "glGenBuffers");
		glDeleteBuffers = (PFNGLDELETEBUFFERSARBPROC)GetProcAddress(glModule, "glDeleteBuffers");
		glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)GetProcAddress(glModule, "glDrawArraysInstanced");
		glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)GetProcAddress(glModule, "glVertexAttribDivisor");
		glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERARBPROC)GetProcAddress(glModule, "glVertexAttribPointer");
		glCreateProgram = (PFNGLCREATEPROGRAMPROC)GetProcAddress(glModule, "glCreateProgram");
		glUseProgram = (PFNGLUSEPROGRAMPROC)GetProcAddress(glModule, "glUseProgram");
		glCreateShader = (PFNGLCREATESHADERPROC)GetProcAddress(glModule, "glCreateShader");
		glShaderSource = (PFNGLSHADERSOURCEPROC)GetProcAddress(glModule, "glShaderSource");
		glCompileShader = (PFNGLCOMPILESHADERPROC)GetProcAddress(glModule, "glCompileShader");
		glAttachShader = (PFNGLATTACHSHADERPROC)GetProcAddress(glModule, "glAttachShader");
		glLinkProgram = (PFNGLLINKPROGRAMPROC)GetProcAddress(glModule, "glLinkProgram");
		glBufferSubData = (PFNGLBUFFERSUBDATAPROC)GetProcAddress(glModule, "glBufferSubData");
		glDrawArrays = (PFNGLDRAWARRAYSPROC)GetProcAddress(glModule, "glDrawArrays");
	}

	void SetupGLHooks(HMODULE hModule, HMODULE glModule) {
		ULONG size;

		for (PIMAGE_IMPORT_DESCRIPTOR desc = (PIMAGE_IMPORT_DESCRIPTOR)::ImageDirectoryEntryToData(hModule, TRUE,
			IMAGE_DIRECTORY_ENTRY_IMPORT, &size); desc->Name != 0; desc++) {
			LPSTR pszMod = (LPSTR)((DWORD)hModule + desc->Name);
			if (_stricmp(pszMod, "libglesv2.dll") == 0) {
				for (PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(desc->FirstThunk + (DWORD)hModule); thunk->u1.Function != 0; thunk++) {
					void** p = (void**)thunk->u1.Function;
					if (*p == glUseProgram)
					{
						DWORD oldProtect;
						::VirtualProtect(p, sizeof(DWORD), PAGE_READWRITE, &oldProtect);
					//	*p = Hook_glUseProgram;
						::VirtualProtect(p, sizeof(DWORD), oldProtect, 0);

						return;
					}
				}
			}
		}
	}

	void SetupClientHooks(HMODULE hModule) {
		// Hook DrawArraysSprite
		const unsigned char startCode[] = { 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x8B, 0x50, 0x08, 0x57, 0xFF, 0xD2, 0x8B, 0x44, 0x24, 0x0C, 0x50, 0x6A, 0x04, 0x8B, 0xCE, 0xE8 };
		const unsigned char endCode[] = { 0x5F, 0x5E, 0xC2, 0x10, 0x00 };
		IMAGE_NT_HEADERS* header = ::ImageNtHeader(hModule);

		for (BYTE* p = (BYTE*)hModule + header->OptionalHeader.BaseOfCode; p < (BYTE*)hModule + header->OptionalHeader.BaseOfCode + header->OptionalHeader.SizeOfCode - 0x200; p += 16) {
			if (memcmp(p, startCode, sizeof(startCode)) == 0) {
				for (BYTE* q = p; q < p + 0x100; q++) {
					if (memcmp(q, endCode, sizeof(endCode)) == 0) {
						// search for necessary functions
						if (p[0x14] == 0xe8) {
							PrepareVertexBuffer = ForceCast<PFNPREPAREVERTEXBUFFER>(*(DWORD*)&p[0x15] + (DWORD)p + 5 + 0x14);
						}

						if (p[0x1B] == 0xe8) {
							BindVertexBuffer = ForceCast<PFNBINDVERTEXBUFFER>(*(DWORD*)&p[0x1C] + (DWORD)p + 5 + 0x1B);
						}

						if (p[0x2A] == 0xe8) {
							GetBuffer = ForceCast<PFNGETBUFFER>(*(DWORD*)&p[0x2B] + (DWORD)p + 5 + 0x2A);
						}

						if (p[0xA0] == 0xe8) {
							Free = ForceCast<PFNFREE>(*(DWORD*)&p[0xA1] + (DWORD)p + 5 + 0xA0);
						}

						// Match!
						unsigned char jmpCode[5] = { 0xe9, 0, 0, 0, 0 };
						DWORD oldProtect;
						::VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
						*(DWORD*)(jmpCode + 1) = (DWORD)&DrawArraysSprite - 5 - (DWORD)p;
						memcpy(p, jmpCode, 5);
						::VirtualProtect(p, 5, oldProtect, &oldProtect);
						return;
					}
				}
			}
		}


		printf("Setup client hooks failed!\n");
	}

	GLContext() {
		HMODULE glModule = GetModuleHandleA("libglesv2.dll");
		if (glModule == NULL)
			return;

		HMODULE hModule = GetModuleHandleA(NULL);
		GetGLAddresses(glModule);
		SetupGLHooks(hModule, glModule);
		SetupClientHooks(hModule);
	}
} context;

GLContext* _this;
void _stdcall DrawArraysSpriteImpl(Object* object, int first, int count, int primType) {
	ReplaceProgram = true;

	typedef void(__thiscall *PFNBINDMATERIAL)(void*);
	PFNBINDMATERIAL BindMaterial = ForceCast<PFNBINDMATERIAL>(*(DWORD*)(*(const char**)_this + 8)); // 2nd virtual function of Context

	BindMaterial(_this);
	GLContext::PrepareVertexBuffer(_this, 4, object);
	GLContext::BindVertexBuffer(_this);
	void* buffer = GLContext::GetBuffer(*(void**)((char*)_this + 444), *(DWORD*)((char*)_this + 44));

	typedef void(__thiscall *PFNSETMATERIAL)(void*, DWORD, const char*);
	PFNSETMATERIAL SetMaterial = ForceCast<PFNSETMATERIAL>(*(DWORD*)(*(DWORD*)buffer + 8));
	SetMaterial(buffer, *(DWORD*)(_this + 420), (const char*)_this + 16);

	if (*(DWORD *)(_this + 32) != -1) {
		glBindBuffer(34963, 0);
		*(DWORD *)(_this + 32) = -1;
	}

	static int types[] = { 0, 3, 2, 1, 5, 6, 4, 3, 0 };
	glDrawArrays(types[primType], first, count);

	GLContext::Free(_this, 4);
	ReplaceProgram = false;
}

__declspec(naked)void DrawArraysSprite() {
	_asm mov _this, ecx;
	_asm jmp DrawArraysSpriteImpl;
}

GLContext::PFNPREPAREVERTEXBUFFER GLContext::PrepareVertexBuffer = NULL;
GLContext::PFNBINDVERTEXBUFFER GLContext::BindVertexBuffer = NULL;
GLContext::PFNGETBUFFER GLContext::GetBuffer = NULL;
GLContext::PFNFREE GLContext::Free = NULL;


#pragma endregion OpenGL