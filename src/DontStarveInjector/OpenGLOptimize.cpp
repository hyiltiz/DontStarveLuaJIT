#include <Windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#pragma region OpenGL

typedef void(_stdcall* PFNGLBINDBUFFERARBPROC)(int target, unsigned int buffer);
PFNGLBINDBUFFERARBPROC glBindBuffer;

typedef void(_stdcall* PFNGLBUFFERDATAARBPROC)(int target, int size, const void* data, int usage);
PFNGLBUFFERDATAARBPROC glBufferData;

typedef void(_stdcall* PFNGLGENBUFFERSARBPROC)(int n, unsigned int* buffers);
PFNGLGENBUFFERSARBPROC glGenBuffers;

typedef void(_stdcall* PFNGLDELETEBUFFERSARBPROC)(int n, const unsigned int* buffers);
PFNGLDELETEBUFFERSARBPROC glDeleteBuffers;

typedef void (_stdcall* PFNGLDRAWARRAYSINSTANCEDPROC) (int mode, int first, int count, int primcount);
PFNGLDRAWARRAYSINSTANCEDPROC glDrawArraysInstanced;

typedef void (_stdcall* PFNGLVERTEXATTRIBDIVISORPROC) (int index, int divisor);
PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor;

typedef void (_stdcall* PFNGLVERTEXATTRIBPOINTERARBPROC) (int index, int size, int type, bool normalized, int stride, const void* pointer);
PFNGLVERTEXATTRIBPOINTERARBPROC glVertexAttribPointer;

typedef void (_stdcall* PFNGLSHADERSOURCEPROC) (int shader, int count, const char* const* string, const int* length);
PFNGLSHADERSOURCEPROC glShaderSource;

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

void _stdcall Hook_glShaderSource(int shader, int count, const char* strings[], const int* length) {
	/*
	for (int i = 0; i < count; i++) {
		if (strstr(strings[i], "uniform mat4 MatrixW") != nullptr) {
			strings[i] = instancedVertexShader;
		}
	}*/

	glShaderSource(shader, count, strings, length);
}

struct Object {

};

struct GLContext {
	GLContext() {
		HMODULE glModule = GetModuleHandleA("libglesv2.dll");
		if (glModule == NULL)
			return;

		glBindBuffer = (PFNGLBINDBUFFERARBPROC)GetProcAddress(glModule, "glBindBuffer");
		glBufferData = (PFNGLBUFFERDATAARBPROC)GetProcAddress(glModule, "glBufferData");
		glGenBuffers = (PFNGLGENBUFFERSARBPROC)GetProcAddress(glModule, "glGenBuffers");
		glDeleteBuffers = (PFNGLDELETEBUFFERSARBPROC)GetProcAddress(glModule, "glDeleteBuffers");
		glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)GetProcAddress(glModule, "glDrawArraysInstanced");
		glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)GetProcAddress(glModule, "glVertexAttribDivisor");
		glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERARBPROC)GetProcAddress(glModule, "glVertexAttribPointer");
		glShaderSource = (PFNGLSHADERSOURCEPROC)GetProcAddress(glModule, "glShaderSource");

		HMODULE hModule = GetModuleHandleA("dontstarve_steam.exe");
		ULONG size;

		for (PIMAGE_IMPORT_DESCRIPTOR desc = (PIMAGE_IMPORT_DESCRIPTOR)::ImageDirectoryEntryToData(hModule, TRUE,
			IMAGE_DIRECTORY_ENTRY_IMPORT, &size); desc->Name != 0; desc++) {
			LPSTR pszMod = (LPSTR)((DWORD)hModule + desc->Name);
			if (_stricmp(pszMod, "libglesv2.dll") == 0) {
				for (PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(desc->FirstThunk + (DWORD)hModule); thunk->u1.Function != 0; thunk++)
				{
					void** p = (void**)thunk->u1.Function;
					if (*p == glShaderSource)
					{
						DWORD oldProtect;
						::VirtualProtect(p, sizeof(DWORD), PAGE_READWRITE, &oldProtect);
						*p = Hook_glShaderSource;
						::VirtualProtect(p, sizeof(DWORD), oldProtect, 0);

						return;
					}
				}
			}
		}
	}

	void __thiscall DrawArraysSprite(Object* object, int first, int count, int primType)
	{
		typedef void (GLContext::*PFNSETMATERIAL)();
		PFNSETMATERIAL SetMaterial = *(PFNSETMATERIAL*)(*(const char**)this + 8); // 2nd virtual function of Context
		(this->*SetMaterial)();
	}
} context;

#pragma endregion OpenGL