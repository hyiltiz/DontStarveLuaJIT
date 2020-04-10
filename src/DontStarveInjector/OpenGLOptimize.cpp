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

typedef void (_stdcall* PFNGLSHADERSOURCEPROC) (int shader, int count, const char* const* strings, const int* length);
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
		if (strstr(strings[i], "uniform mat4 MatrixW") != NULL) {
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

		HMODULE hModule = GetModuleHandleA(NULL);
		// Hook DrawArraysSprite
		const char startCode[] = { 0x56, 0x8B, 0xF1, 0x8B, 0x06, 0x8B, 0x50, 0x08, 0x57, 0xFF, 0xD2, 0x8B, 0x44, 0x24, 0x0C, 0x50, 0x6A, 0x04, 0x8B, 0xCE, 0xE8 };
		const char endCode[] = { 0x5F, 0x5E, 0xC2, 0x10, 0x00 };
		IMAGE_NT_HEADERS* header = ::ImageNtHeader(hModule);

		for (BYTE* p = (BYTE*)hModule + header->OptionalHeader.BaseOfCode; p < (BYTE*)hModule + header->OptionalHeader.BaseOfCode + header->OptionalHeader.SizeOfCode - 0x200; p += 16) {
			if (memcmp(p, startCode, sizeof(startCode)) == 0) {
				bool match = false;
				for (BYTE* q = p; q < p + 0x100; q++) {
					if (memcmp(q, endCode, sizeof(endCode)) == 0) {
						match = true;
					}
				}

				if (match) {
					// Match!
					char jmpCode[5] = { 0xe9, 0, 0, 0, 0 };
					DWORD oldProtect;
					::VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProtect);
					*(DWORD*)(jmpCode + 1) = (DWORD)&GLContext::DrawArraysSprite - 5 - (DWORD)p;
					memcpy(p, jmpCode, 5);
					::VirtualProtect(p, 5, oldProtect, &oldProtect);
					break;
				}
			}
		}

		glBindBuffer = (PFNGLBINDBUFFERARBPROC)GetProcAddress(glModule, "glBindBuffer");
		glBufferData = (PFNGLBUFFERDATAARBPROC)GetProcAddress(glModule, "glBufferData");
		glGenBuffers = (PFNGLGENBUFFERSARBPROC)GetProcAddress(glModule, "glGenBuffers");
		glDeleteBuffers = (PFNGLDELETEBUFFERSARBPROC)GetProcAddress(glModule, "glDeleteBuffers");
		glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)GetProcAddress(glModule, "glDrawArraysInstanced");
		glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)GetProcAddress(glModule, "glVertexAttribDivisor");
		glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERARBPROC)GetProcAddress(glModule, "glVertexAttribPointer");
		glShaderSource = (PFNGLSHADERSOURCEPROC)GetProcAddress(glModule, "glShaderSource");

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

	static void _stdcall DrawArraysSprite(Object* object, int first, int count, int primType)
	{
		GLContext* _this;
		_asm mov _this, ecx;

		typedef void (GLContext::*PFNSETMATERIAL)();
		PFNSETMATERIAL SetMaterial = *(PFNSETMATERIAL*)(*(const char**)_this + 8); // 2nd virtual function of Context
		//(_this->*SetMaterial)();
	}
} context;

#pragma endregion OpenGL