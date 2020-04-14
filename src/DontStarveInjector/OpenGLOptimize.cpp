#include <Windows.h>
#include <dbghelp.h>
#include <vector>
#include <unordered_map>
#include <cassert>
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

typedef void (_stdcall* PFNGLGETSHADERINFOLOGPROC) (int shader, int bufSize, int* length, char* infoLog);
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;

typedef void (_stdcall* PFNGLBUFFERSUBDATAPROC) (int target, int offset, int size, const void* data);
PFNGLBUFFERSUBDATAPROC glBufferSubData;

typedef void (_stdcall* PFNGLDRAWARRAYSPROC) (int, int, int);
PFNGLDRAWARRAYSPROC glDrawArrays;

typedef int(_stdcall* PFNGLGETATTRIBLOCATION)(int program, const char* name);
PFNGLGETATTRIBLOCATION glGetAttribLocation;

typedef int(_stdcall* PFNGLENABLEVERTEXATTRIBARRAYPROC)(int index);
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;

typedef int(_stdcall* PFNGLGETERRORPROC)();
PFNGLGETERRORPROC glGetError;

typedef int(_stdcall* PFNEGLSWAPBUFFERSPROC)(int display, int surface);
PFNEGLSWAPBUFFERSPROC eglSwapBuffers;

static bool ReplaceProgram = false;
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_ARRAY_BUFFER 0x8892
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_FLOAT        0x1406

struct Matrix {
	operator float* () {
		return &value[0][0];
	}

	float value[4][4];
};

struct Vector {
	float value[4];
};

std::vector<Vector> instancedBufferData(1024 * 64);
size_t updatedInstancedBufferCount = 0;
size_t instancedBufferCount = 0;
int instancedBuffer = 0;
bool bufferSizeChanged = false;

static void CheckError() {
	// assert(glGetError() == 0);
}

static void _stdcall Hook_eglSwapBuffers(int display, int surface) {
	// finish a frame
	instancedBufferCount = 0;
	updatedInstancedBufferCount = 0;
	eglSwapBuffers(display, surface);
}

static void AddSpriteWorldMatrix(float* data) {
	if (instancedBufferCount > instancedBufferData.size()) {
		bufferSizeChanged = true;
		instancedBufferData.resize(instancedBufferData.size() * 2);
	}

	float* v = (float*)instancedBufferData[instancedBufferCount++].value;
	v[0] = v[1] = 1.0f;
	v[2] = data[3];
	v[3] = data[7];
}

static void UpdateWorldMatrices() {
	if (updatedInstancedBufferCount != instancedBufferCount) {
		CheckError();
		if (instancedBuffer == 0) {
			glGenBuffers(1, &instancedBuffer);
			glBindBuffer(GL_ARRAY_BUFFER, instancedBuffer);
			glBufferData(GL_ARRAY_BUFFER, sizeof(Vector) * instancedBufferData.size(), &instancedBufferData[0], GL_DYNAMIC_DRAW);
		} else {
			glBindBuffer(GL_ARRAY_BUFFER, instancedBuffer);
			if (bufferSizeChanged) {
				glBufferData(GL_ARRAY_BUFFER, sizeof(Vector) * instancedBufferData.size(), &instancedBufferData[0], GL_DYNAMIC_DRAW);
				bufferSizeChanged = false;
			} else {
				glBufferSubData(GL_ARRAY_BUFFER, updatedInstancedBufferCount * sizeof(Vector), (instancedBufferCount - updatedInstancedBufferCount) * sizeof(Vector), &instancedBufferData[0]);
			}
		}

		CheckError();
		updatedInstancedBufferCount = instancedBufferCount;
	}
}

struct ShaderInfo {
	int type;
	std::string code;
};

std::tr1::unordered_map<int, ShaderInfo> mapShaderToInfo;

struct ProgramInfo {
	int vs;
	int fs;
	int ivs;
	int ip;
	int slot;
} *currentProgram;

std::tr1::unordered_map<int, ProgramInfo> mapProgramToInfo;

int _stdcall Hook_glCreateShader(int type) {
	int shader = glCreateShader(type);
	mapShaderToInfo[shader].type = type;
	assert(type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER);
	return shader;
}

void _stdcall Hook_glShaderSource(int shader, int count, const char* strings[], int* v) {
	assert(count == 1);
	mapShaderToInfo[shader].code = strings[0];
	glShaderSource(shader, count, strings, v);
}

void _stdcall Hook_glAttachShader(int program, int shader) {
	ProgramInfo& info = mapProgramToInfo[program];
	assert(mapShaderToInfo[shader].type == GL_VERTEX_SHADER || mapShaderToInfo[shader].type == GL_FRAGMENT_SHADER);
	if (mapShaderToInfo[shader].type == GL_VERTEX_SHADER) {
		info.vs = shader;
	} else {
		info.fs = shader;
	}

	glAttachShader(program, shader);
}

std::string replace(const std::string& t, const std::string& pattern, const std::string& target) {
	size_t offset = 0;
	std::string ret = t;
	while (true) {
		size_t position = ret.find(pattern, offset);
		if (position == std::string::npos) break;

		std::string next = ret.substr(0, position);
		next += target;
		next += ret.substr(position + pattern.length());
		offset = position + target.length();
		std::swap(ret, next);
	}
	
	return ret;
}

void _stdcall Hook_glLinkProgram(int program) {
	ProgramInfo& info = mapProgramToInfo[program];
	glLinkProgram(program);

	ShaderInfo& sinfo = mapShaderToInfo[info.vs];

	if (sinfo.code.find("mat4 mtxPVW = MatrixP * MatrixV * MatrixW;") != std::string::npos) {
		std::string code = replace(sinfo.code, "void main", "attribute vec4 instanced; void main");
		code = replace(code, "mat4 mtxPVW = MatrixP * MatrixV * MatrixW;", "mat4 mtxPVW = MatrixP * MatrixV * MatrixW * mat4(instanced.x, 0, 0, 0, 0, instanced.y, 0, 0, 0, 0, 1, 0, instanced.z, instanced.w, 0, 1);");
		code = replace(code, "mat4 mat = fastanim_xform", "mat4 mat = fastanim_xform * mat4(1.0 + instanced.x * 0.0001, 0, 0, 0, 0, 1.0 + instanced.y * 0.001, 0, 0, 0, 0, 1, 0, instanced.z * 0.0, instanced.w * 0.0, 0, 1)");

		info.ip = glCreateProgram();
		info.ivs = glCreateShader(GL_VERTEX_SHADER);
		const char* source[] = { code.c_str() };
		glShaderSource(info.ivs, 1, source, NULL);
		glCompileShader(info.ivs);
		glAttachShader(info.ip, info.ivs);
		glAttachShader(info.ip, info.fs);
		const int MAX_INFO_LOG_SIZE = 4096;
		char g[MAX_INFO_LOG_SIZE] = { 0 };
		glGetShaderInfoLog(info.ivs, MAX_INFO_LOG_SIZE - 1, nullptr, g);

		glLinkProgram(info.ip);
		info.slot = glGetAttribLocation(info.ip, "instanced");
		assert(info.slot != -1);
	} else {
		info.ip = 0;
	}
}

void _stdcall Hook_glUseProgram(int program) {
	if (ReplaceProgram) {
		ProgramInfo* info = &mapProgramToInfo[program];
		if (info->ip != 0) {
			currentProgram = info;
			glUseProgram(currentProgram->ip);
		} else {
			currentProgram = NULL;
			glUseProgram(program);
		}
	} else {
		glUseProgram(program);
	}
}

static void BindWorldMatrices(int offset) {
	int slot = currentProgram->slot;
	CheckError();
	glEnableVertexAttribArray(slot);
	glBindBuffer(GL_ARRAY_BUFFER, instancedBuffer);
	CheckError();
	glVertexAttribPointer(slot, 4, GL_FLOAT, false, 0, (void*)(offset * sizeof(Vector)));
	CheckError();
	glVertexAttribDivisor(slot, 1);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	CheckError();
	// glVertexAttribDivisor(slot, 0);
}

struct Object {
	Matrix worldTransform;
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
	static PFNPREPAREVERTEXBUFFER PrepareMatrixTransposed;

	typedef void (__thiscall *PFNBINDVERTEXBUFFER)(void*);
	static PFNBINDVERTEXBUFFER BindVertexBuffer;

	typedef void* (__thiscall *PFNGETBUFFER)(void*, DWORD);
	static PFNGETBUFFER GetBuffer;
	
	typedef void (__thiscall *PFNFREE)(void*, int);
	static PFNFREE Free;


	void GetGLAddresses(HMODULE glModule, HMODULE eglModule) {
		glBindBuffer = (PFNGLBINDBUFFERARBPROC)GetProcAddress(glModule, "glBindBuffer");
		glBufferData = (PFNGLBUFFERDATAARBPROC)GetProcAddress(glModule, "glBufferData");
		glGenBuffers = (PFNGLGENBUFFERSARBPROC)GetProcAddress(glModule, "glGenBuffers");
		glDeleteBuffers = (PFNGLDELETEBUFFERSARBPROC)GetProcAddress(glModule, "glDeleteBuffers");
		glDrawArraysInstanced = (PFNGLDRAWARRAYSINSTANCEDPROC)GetProcAddress(glModule, "glDrawArraysInstancedANGLE");
		glVertexAttribDivisor = (PFNGLVERTEXATTRIBDIVISORPROC)GetProcAddress(glModule, "glVertexAttribDivisorANGLE");
		glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERARBPROC)GetProcAddress(glModule, "glVertexAttribPointer");
		glCreateProgram = (PFNGLCREATEPROGRAMPROC)GetProcAddress(glModule, "glCreateProgram");
		glUseProgram = (PFNGLUSEPROGRAMPROC)GetProcAddress(glModule, "glUseProgram");
		glCreateShader = (PFNGLCREATESHADERPROC)GetProcAddress(glModule, "glCreateShader");
		glShaderSource = (PFNGLSHADERSOURCEPROC)GetProcAddress(glModule, "glShaderSource");
		glCompileShader = (PFNGLCOMPILESHADERPROC)GetProcAddress(glModule, "glCompileShader");
		glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)GetProcAddress(glModule, "glGetShaderInfoLog");
		glAttachShader = (PFNGLATTACHSHADERPROC)GetProcAddress(glModule, "glAttachShader");
		glLinkProgram = (PFNGLLINKPROGRAMPROC)GetProcAddress(glModule, "glLinkProgram");
		glBufferSubData = (PFNGLBUFFERSUBDATAPROC)GetProcAddress(glModule, "glBufferSubData");
		glDrawArrays = (PFNGLDRAWARRAYSPROC)GetProcAddress(glModule, "glDrawArrays");
		glGetAttribLocation = (PFNGLGETATTRIBLOCATION)GetProcAddress(glModule, "glGetAttribLocation");
		glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)GetProcAddress(glModule, "glEnableVertexAttribArray");
		glGetError = (PFNGLGETERRORPROC)GetProcAddress(glModule, "glGetError");

		eglSwapBuffers = (PFNEGLSWAPBUFFERSPROC)GetProcAddress(eglModule, "eglSwapBuffers");
	}

	void SetupGLHooks(HMODULE hModule, HMODULE glModule) {
		ULONG size;

		void* addresses[] = {
			glUseProgram, Hook_glUseProgram,
			glCreateShader, Hook_glCreateShader,
			glShaderSource, Hook_glShaderSource,
			glAttachShader, Hook_glAttachShader,
			glLinkProgram, Hook_glLinkProgram,
			eglSwapBuffers, Hook_eglSwapBuffers,
		};

		size_t count = 0;
		for (PIMAGE_IMPORT_DESCRIPTOR desc = (PIMAGE_IMPORT_DESCRIPTOR)::ImageDirectoryEntryToData(hModule, TRUE,
			IMAGE_DIRECTORY_ENTRY_IMPORT, &size); desc->Name != 0; desc++) {
			LPSTR pszMod = (LPSTR)((DWORD)hModule + desc->Name);
			if (_stricmp(pszMod, "libglesv2.dll") == 0 || _stricmp(pszMod, "libEGL.dll") == 0) {
				for (PIMAGE_THUNK_DATA thunk = (PIMAGE_THUNK_DATA)(desc->FirstThunk + (DWORD)hModule); thunk->u1.Function != 0; thunk++) {
					void*& p = (void*&)thunk->u1.Function;
					for (size_t i = 0; i < sizeof(addresses) / sizeof(addresses[0]); i += 2) {
						if (p == addresses[i]) {
							DWORD oldProtect;
							::VirtualProtect(&p, sizeof(DWORD), PAGE_EXECUTE_READWRITE, &oldProtect);
							p = addresses[i + 1];
							::VirtualProtect(&p, sizeof(DWORD), oldProtect, 0);
							if (++count == sizeof(addresses) / sizeof(addresses[0]) / 2) {
								return;
							}
						}
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
							PrepareMatrixTransposed = ForceCast<PFNPREPAREVERTEXBUFFER>(*(DWORD*)&p[0x15] + (DWORD)p + 5 + 0x14);
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
		HMODULE eglModule = GetModuleHandleA("libEGL.dll");
		if (eglModule == NULL)
			return;

		HMODULE hModule = GetModuleHandleA(NULL);
		GetGLAddresses(glModule, eglModule);
		SetupGLHooks(hModule, glModule);
		SetupClientHooks(hModule);
	}
} context;

GLContext* _this;
void _stdcall DrawArraysSpriteImpl(Object* object, int first, int count, int primType) {
	ReplaceProgram = true;

	typedef void(__thiscall *PFNBINDMATERIAL)(void*);
	PFNBINDMATERIAL BindMaterial = ForceCast<PFNBINDMATERIAL>(*(DWORD*)(*(const char**)_this + 8)); // 2nd virtual function of Context

	int e = glGetError(); // eat error
	BindMaterial(_this);
	CheckError();

	if (currentProgram != NULL) {
		size_t current = updatedInstancedBufferCount;
		CheckError();
		AddSpriteWorldMatrix(object->worldTransform);
		CheckError();
		UpdateWorldMatrices();
		CheckError();
		BindWorldMatrices(current);
		CheckError();
	}

	GLContext::PrepareMatrixTransposed(_this, 4, object);
		CheckError();
	GLContext::BindVertexBuffer(_this);
		CheckError();
	void* buffer = GLContext::GetBuffer(*(void**)((char*)_this + 444), *(DWORD*)((char*)_this + 44));

	typedef void(__thiscall *PFNSETMATERIAL)(void*, DWORD, const char*);
	PFNSETMATERIAL SetMaterial = ForceCast<PFNSETMATERIAL>(*(DWORD*)(*(DWORD*)buffer + 8));
	SetMaterial(buffer, *(DWORD*)(_this + 420), (const char*)_this + 16);

	if (*(DWORD *)(_this + 32) != -1) {
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		*(DWORD *)(_this + 32) = -1;
	}

	static int types[] = { 0, 3, 2, 1, 5, 6, 4, 3, 0 };
	if (currentProgram != NULL) {
		// glDrawArrays(types[primType], first, count);
		CheckError();
		glDrawArraysInstanced(types[primType], first, count, 1);
		CheckError();
	} else {
		glDrawArrays(types[primType], first, count);
	}

	GLContext::Free(_this, 4);
	ReplaceProgram = false;
}

__declspec(naked)void DrawArraysSprite() {
	_asm mov _this, ecx;
	_asm jmp DrawArraysSpriteImpl;
}

GLContext::PFNPREPAREVERTEXBUFFER GLContext::PrepareMatrixTransposed = NULL;
GLContext::PFNBINDVERTEXBUFFER GLContext::BindVertexBuffer = NULL;
GLContext::PFNGETBUFFER GLContext::GetBuffer = NULL;
GLContext::PFNFREE GLContext::Free = NULL;


#pragma endregion OpenGL