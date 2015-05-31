#ifndef UNICODE
#define UNICODE
#endif

#pragma comment(lib,"gdiplus")
#pragma comment(lib,"glew32s")
#pragma comment(lib,"libx264")
#pragma comment(lib,"shlwapi")
#pragma comment(lib,"rpcrt4")

#define GLEW_STATIC

#include <vector>
#include <string>
#include <windows.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include <richedit.h>
#include <GL/glew.h>
#include <GL/glut.h>

#include <stdint.h>
extern "C" {
#include "x264.h"
}

#include "resource.h"

#define PREVIEW_WIDTH 512
#define PREVIEW_HEIGHT 384
#define ID_SELECTALL 1001
#define WM_CREATED WM_APP

HDC hDC;
BOOL active;
GLuint program;
GLuint vao;
GLuint vbo;
const TCHAR szClassName[] = TEXT("Window");
const GLfloat position[][2] = { { -1.f, -1.f }, { 1.f, -1.f }, { 1.f, 1.f }, { -1.f, 1.f } };
const int vertices = sizeof position / sizeof position[0];
const GLchar vsrc[] = "in vec4 position;void main(void){gl_Position = position;}";
GLuint texture;

// RGB形式からYUV形式に変換
int RGB2YUV(int x_dim, int y_dim, void *bmp, void *y_out, void *u_out, void *v_out)
{
	static int init_done = 0;
	static float RGBYUV02990[256], RGBYUV05870[256], RGBYUV01140[256];
	static float RGBYUV01684[256], RGBYUV03316[256];
	static float RGBYUV04187[256], RGBYUV00813[256];
	long i, j, size;
	unsigned char *r, *g, *b;
	unsigned char *y, *u, *v;
	unsigned char *pu1, *pu2, *pv1, *pv2, *psu, *psv;
	unsigned char *y_buffer, *u_buffer, *v_buffer;
	unsigned char *sub_u_buf, *sub_v_buf;
	if (init_done == 0)
	{
		for (i = 0; i < 256; i++) RGBYUV02990[i] = (float)0.2990 * i;
		for (i = 0; i < 256; i++) RGBYUV05870[i] = (float)0.5870 * i;
		for (i = 0; i < 256; i++) RGBYUV01140[i] = (float)0.1140 * i;
		for (i = 0; i < 256; i++) RGBYUV01684[i] = (float)0.1684 * i;
		for (i = 0; i < 256; i++) RGBYUV03316[i] = (float)0.3316 * i;
		for (i = 0; i < 256; i++) RGBYUV04187[i] = (float)0.4187 * i;
		for (i = 0; i < 256; i++) RGBYUV00813[i] = (float)0.0813 * i;
		init_done = 1;
	}
	// check to see if x_dim and y_dim are divisible by 2
	if ((x_dim % 2) || (y_dim % 2)) return 1;
	size = x_dim * y_dim;
	// allocate memory
	y_buffer = (unsigned char *)y_out;
	sub_u_buf = (unsigned char *)u_out;
	sub_v_buf = (unsigned char *)v_out;
	u_buffer = (unsigned char *)GlobalAlloc(0, size * sizeof(unsigned char));
	v_buffer = (unsigned char *)GlobalAlloc(0, size * sizeof(unsigned char));
	if (!(u_buffer && v_buffer))
	{
		if (u_buffer) GlobalFree(u_buffer);
		if (v_buffer) GlobalFree(v_buffer);
		return 2;
	}
	r = (unsigned char *)bmp;
	y = y_buffer;
	u = u_buffer;
	v = v_buffer;
	for (j = 0; j < y_dim; j++)
	{
		y = y_buffer + (y_dim - j - 1) * x_dim;
		u = u_buffer + (y_dim - j - 1) * x_dim;
		v = v_buffer + (y_dim - j - 1) * x_dim;
		for (i = 0; i < x_dim; i++)
		{
			g = r + 1;
			b = r + 2;
			*y = (unsigned char)(RGBYUV02990[*r] + RGBYUV05870[*g] + RGBYUV01140[*b]);
			*u = (unsigned char)(-RGBYUV01684[*r] - RGBYUV03316[*g] + (*b) / 2 + 128);
			*v = (unsigned char)((*r) / 2 - RGBYUV04187[*g] - RGBYUV00813[*b] + 128);
			r += 3;
			y++;
			u++;
			v++;
		}
	}
	for (j = 0; j < y_dim / 2; j++)
	{
		psu = sub_u_buf + j * x_dim / 2;
		psv = sub_v_buf + j * x_dim / 2;
		pu1 = u_buffer + 2 * j * x_dim;
		pu2 = u_buffer + (2 * j + 1) * x_dim;
		pv1 = v_buffer + 2 * j * x_dim;
		pv2 = v_buffer + (2 * j + 1) * x_dim;
		for (i = 0; i < x_dim / 2; i++)
		{
			*psu = (*pu1 + *(pu1 + 1) + *pu2 + *(pu2 + 1)) / 4;
			*psv = (*pv1 + *(pv1 + 1) + *pv2 + *(pv2 + 1)) / 4;
			psu++;
			psv++;
			pu1 += 2;
			pu2 += 2;
			pv1 += 2;
			pv2 += 2;
		}
	}
	GlobalFree(u_buffer);
	GlobalFree(v_buffer);
	return 0;
}

inline GLint GetShaderInfoLog(GLuint shader)
{
	GLint status;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
	if (status == 0) OutputDebugString(TEXT("Compile Error\n"));
	GLsizei bufSize;
	glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &bufSize);
	if (bufSize > 1)
	{
		LPSTR infoLog = (LPSTR)GlobalAlloc(0, bufSize);
		GLsizei length;
		glGetShaderInfoLog(shader, bufSize, &length, infoLog);
		OutputDebugStringA(infoLog);
		GlobalFree(infoLog);
	}
	return status;
}

inline GLint GetProgramInfoLog(GLuint program)
{
	GLint status;
	glGetProgramiv(program, GL_LINK_STATUS, &status);
	if (status == 0) OutputDebugString(TEXT("Link Error\n"));
	GLsizei bufSize;
	glGetProgramiv(program, GL_INFO_LOG_LENGTH, &bufSize);
	if (bufSize > 1)
	{
		LPSTR infoLog = (LPSTR)GlobalAlloc(0, bufSize);
		GLsizei length;
		glGetProgramInfoLog(program, bufSize, &length, infoLog);
		OutputDebugStringA(infoLog);
		GlobalFree(infoLog);
	}
	return status;
}

inline GLuint CreateProgram(LPCSTR vsrc, LPCSTR fsrc)
{
	const GLuint vobj = glCreateShader(GL_VERTEX_SHADER);
	if (!vobj) return 0;
	glShaderSource(vobj, 1, &vsrc, 0);
	glCompileShader(vobj);
	if (GetShaderInfoLog(vobj) == 0)
	{
		glDeleteShader(vobj);
		return 0;
	}
	const GLuint fobj = glCreateShader(GL_FRAGMENT_SHADER);
	if (!fobj)
	{
		glDeleteShader(vobj);
		return 0;
	}
	glShaderSource(fobj, 1, &fsrc, 0);
	glCompileShader(fobj);
	if (GetShaderInfoLog(fobj) == 0)
	{
		glDeleteShader(vobj);
		glDeleteShader(fobj);
		return 0;
	}
	GLuint program = glCreateProgram();
	if (program)
	{
		glAttachShader(program, vobj);
		glAttachShader(program, fobj);
		glLinkProgram(program);
		if (GetProgramInfoLog(program) == 0)
		{
			glDetachShader(program, fobj);
			glDetachShader(program, vobj);
			glDeleteProgram(program);
			program = 0;
		}
	}
	glDeleteShader(vobj);
	glDeleteShader(fobj);
	return program;
}

inline BOOL InitGL(GLvoid)
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(GLfloat) * 2 *
		vertices, position, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 2, GL_FLOAT, 0, 0, 0);
	glEnableVertexAttribArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);
	return TRUE;
}

inline VOID DrawGLScene()
{
	glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glUseProgram(program);
	glUniform1f(glGetUniformLocation(program, "time"), GetTickCount() / 1000.0f);

	if (texture)
	{
		glOrtho(-PREVIEW_WIDTH / 2.0f, PREVIEW_WIDTH / 2.0f, PREVIEW_HEIGHT / 2.0f, -PREVIEW_HEIGHT / 2.0f, -0.1, 0.1);
		glEnable(GL_TEXTURE_2D);
		glPushMatrix();
		glTranslatef((GLfloat)0, (GLfloat)0, 0.0f);
		glRotatef((GLfloat)0, 0, 0, 1.0f);
		glScalef((GLfloat)1.0f, (GLfloat)1.0f, (GLfloat)1.0f);
		glColor4f(1.0f, 1.0f, 1.0f, (GLfloat)1.0f);
		glBindTexture(GL_TEXTURE_2D, texture);
		glBegin(GL_POLYGON);
		glTexCoord2f(0, 0); glVertex2f((GLfloat)(-PREVIEW_WIDTH / 2.0f), (GLfloat)(-PREVIEW_HEIGHT / 2.0f));
		glTexCoord2f(0, 1); glVertex2f((GLfloat)(-PREVIEW_WIDTH / 2.0f), (GLfloat)(PREVIEW_HEIGHT / 2.0f));
		glTexCoord2f(1, 1); glVertex2f((GLfloat)(PREVIEW_WIDTH / 2.0f), (GLfloat)(PREVIEW_HEIGHT / 2.0f));
		glTexCoord2f(1, 0); glVertex2f((GLfloat)(PREVIEW_WIDTH / 2.0f), (GLfloat)(-PREVIEW_HEIGHT / 2.0f));
		glEnd();
		glPopMatrix();
		glDisable(GL_TEXTURE_2D);
	}
	else
	{
		glBindVertexArray(vao);
		glDrawArrays(GL_QUADS, 0, vertices);
		glBindVertexArray(0);
	}
	glUseProgram(0);
	glFlush();
	SwapBuffers(hDC);
}

inline VOID DrawGLScene(GLfloat time)
{
	glClear(GL_COLOR_BUFFER_BIT);
	glUseProgram(program);
	glUniform1f(glGetUniformLocation(program, "time"), time);
	glBindVertexArray(vao);
	glDrawArrays(GL_QUADS, 0, vertices);
	glBindVertexArray(0);
	glUseProgram(0);
	glFlush();
	SwapBuffers(hDC);
}

// GUIDの生成
inline BOOL CreateGUID(OUT LPTSTR lpszGUID)
{
	GUID guid = GUID_NULL;
	HRESULT hr = UuidCreate(&guid);
	if (HRESULT_CODE(hr) != RPC_S_OK) return FALSE;
	if (guid == GUID_NULL) return FALSE;
	wsprintf(lpszGUID, TEXT("{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}"),
		guid.Data1, guid.Data2, guid.Data3,
		guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	return TRUE;
}

// リソースをファイルに出力
inline BOOL CreateFileFromResource(IN LPCTSTR lpszResourceName, IN LPCTSTR lpszResourceType,
	IN LPCTSTR lpszResFileName)
{
	DWORD dwWritten;
	const HRSRC hRs = FindResource(GetModuleHandle(0), lpszResourceName, lpszResourceType);
	if (!hRs) return FALSE;
	const DWORD dwResSize = SizeofResource(0, hRs);
	if (!dwResSize) return FALSE;
	const HGLOBAL hMem = LoadResource(0, hRs);
	if (!hMem) return FALSE;
	const LPBYTE lpByte = (BYTE *)LockResource(hMem);
	if (!lpByte) return FALSE;
	const HANDLE hFile = CreateFile(lpszResFileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) return FALSE;
	BOOL bReturn = FALSE;
	if (0 != WriteFile(hFile, lpByte, dwResSize, &dwWritten, NULL))
	{
		bReturn = TRUE;
	}
	CloseHandle(hFile);
	return bReturn;
}

// Tempフォルダにディレクトリを作成
inline BOOL CreateTempDirectory(OUT LPTSTR pszDir)
{
	TCHAR szGUID[39];
	if (GetTempPath(MAX_PATH, pszDir) == 0)return FALSE;
	if (CreateGUID(szGUID) == 0)return FALSE;
	if (PathAppend(pszDir, szGUID) == 0)return FALSE;
	if (CreateDirectory(pszDir, 0) == 0)return FALSE;
	return TRUE;
}

// OpenGLの描画をH264形式として出力
inline BOOL CreateH264(LPCTSTR lpszOutputFilePath, DWORD dwSecond)
{
	x264_param_t param;
	x264_t* encoder;
	x264_picture_t pic_in;
	x264_picture_t pic_out;
	FILE* pFile;
	_wfopen_s(&pFile, lpszOutputFilePath, L"wb");
	x264_param_default_preset(&param, "veryfast", "zerolatency");
	param.i_threads = 8;
	param.i_width = PREVIEW_WIDTH;
	param.i_height = PREVIEW_HEIGHT;
	param.i_fps_num = 30;
	param.i_fps_den = 1;
	param.i_keyint_max = 30;
	param.b_intra_refresh = 1;
	param.rc.i_rc_method = X264_RC_CRF;
	param.rc.f_rf_constant = 25;
	param.rc.f_rf_constant_max = 35;
	param.b_annexb = 1;
	param.b_repeat_headers = 1;
	param.i_log_level = X264_LOG_NONE;
	x264_param_apply_profile(&param, "baseline");
	encoder = x264_encoder_open(&param);
	x264_picture_alloc(&pic_in, X264_CSP_I420, param.i_width, param.i_height);
	x264_encoder_parameters(encoder, &param);
	GLubyte *data = (GLubyte*)GlobalAlloc(0, 3 * param.i_width * param.i_height);
	GLubyte *PixelYUV = (GLubyte*)GlobalAlloc(0, 3 * param.i_width * param.i_height);
	x264_nal_t* nals;
	int i_nals;
	int frame_size;
	uint8_t *p1 = pic_in.img.plane[0];
	uint8_t *p2 = pic_in.img.plane[1];
	uint8_t *p3 = pic_in.img.plane[2];
	for (DWORD i = 0; i < param.i_fps_num * dwSecond; i++)
	{
		DrawGLScene(i / 30.f);
		glReadPixels(0, 0, param.i_width, param.i_height, GL_RGB, GL_UNSIGNED_BYTE, data);
		RGB2YUV(param.i_width, param.i_height, data, PixelYUV, PixelYUV + param.i_width * param.i_height, PixelYUV + param.i_width * param.i_height + (param.i_width * param.i_height) / 4);
		pic_in.img.plane[0] = PixelYUV;
		pic_in.img.plane[1] = PixelYUV + param.i_width * param.i_height;
		pic_in.img.plane[2] = PixelYUV + param.i_width * param.i_height + (param.i_width * param.i_height) / 4;
		frame_size = x264_encoder_encode(encoder, &nals, &i_nals, &pic_in, &pic_out);
		if (frame_size)
		{
			fwrite((char*)nals[0].p_payload, frame_size, 1, pFile);
		}
	}
	pic_in.img.plane[0] = p1;
	pic_in.img.plane[1] = p2;
	pic_in.img.plane[2] = p3;
	x264_encoder_close(encoder);
	GlobalFree(PixelYUV);
	GlobalFree(data);
	x264_picture_clean(&pic_in);
	fclose(pFile);
	return TRUE;
}

// FFMPEGを使ってH264形式の動画をMP4形式に変換
inline BOOL OutputMP4fromH264(LPCTSTR lpszffmpegFilePath, LPCTSTR lpszInputH264FilePath, LPCTSTR lpszOutputFilePath)
{
	PROCESS_INFORMATION pi = { 0 };
	STARTUPINFO si = { 0 };
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	TCHAR szCommand[1024];
	lstrcpy(szCommand, TEXT("\""));
	lstrcat(szCommand, lpszffmpegFilePath);
	lstrcat(szCommand, TEXT("\" -y -i \""));
	lstrcat(szCommand, lpszInputH264FilePath);
	lstrcat(szCommand, TEXT("\" -vcodec copy -an \""));
	lstrcat(szCommand, lpszOutputFilePath);
	lstrcat(szCommand, TEXT("\""));
	CreateProcess(0, szCommand, 0, 0, 0, NORMAL_PRIORITY_CLASS, 0, 0, &si, &pi);
	CloseHandle(pi.hThread);
	WaitForSingleObject(pi.hProcess, INFINITE);
	CloseHandle(pi.hProcess);
	return TRUE;
}

GLuint LoadImage(LPCTSTR lpszFilePath)
{
	GLuint texture = 0;
	Gdiplus::Bitmap bitmap(lpszFilePath);
	if (bitmap.GetLastStatus() == Gdiplus::Ok)
	{
		Gdiplus::BitmapData data;
		bitmap.LockBits(0, Gdiplus::ImageLockModeRead, PixelFormat32bppARGB, &data);
		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexImage2D(GL_TEXTURE_2D,
			0, GL_RGBA, data.Width, data.Height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data.Scan0);
		glBindTexture(GL_TEXTURE_2D, 0);
		bitmap.UnlockBits(&data);
	}
	return texture;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static PIXELFORMATDESCRIPTOR pfd = { sizeof(PIXELFORMATDESCRIPTOR), 1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, PFD_TYPE_RGBA,
		32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 16, 0, 0, PFD_MAIN_PLANE, 0, 0, 0, 0 };
	static GLuint PixelFormat;
	static HWND hStatic;
	static HWND hEdit;
	static HWND hButton;
	static HFONT hFont;
	static HINSTANCE hRtLib;
	static BOOL bEditVisible = TRUE;
	static HGLRC hRC;
	switch (msg)
	{
	case WM_CREATE:
		hRtLib = LoadLibrary(TEXT("RICHED32"));
		hFont = CreateFont(24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, TEXT("Consolas"));
		hStatic = CreateWindow(TEXT("STATIC"), 0, WS_VISIBLE | WS_CHILD | SS_SIMPLE,
			10, 10, PREVIEW_WIDTH, PREVIEW_HEIGHT, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hEdit = CreateWindowEx(WS_EX_CLIENTEDGE, RICHEDIT_CLASS, 0, WS_VISIBLE | WS_CHILD | WS_HSCROLL |
			WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
			0, 0, 0, 0, hWnd, 0, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		hButton = CreateWindow(TEXT("BUTTON"), TEXT("MP4出力..."), WS_VISIBLE | WS_CHILD,
			PREVIEW_WIDTH / 2 - 54, PREVIEW_HEIGHT + 20, 128, 32, hWnd, (HMENU)100, ((LPCREATESTRUCT)lParam)->hInstance, 0);
		SendMessage(hEdit, EM_SETTEXTMODE, TM_PLAINTEXT, 0);
		SendMessage(hEdit, EM_LIMITTEXT, -1, 0);
		SendMessage(hEdit, WM_SETFONT, (WPARAM)hFont, 0);
		if (!(hDC = GetDC(hStatic)) ||
			!(PixelFormat = ChoosePixelFormat(hDC, &pfd)) ||
			!SetPixelFormat(hDC, PixelFormat, &pfd) ||
			!(hRC = wglCreateContext(hDC)) ||
			!wglMakeCurrent(hDC, hRC) ||
			glewInit() != GLEW_OK ||
			!InitGL()) return -1;
		SetWindowText(hEdit,
			TEXT("#define pi 3.14159265358979\r\n")
			TEXT("uniform sampler2D image;\r\n")
			TEXT("uniform float time;\r\n")
			TEXT("void main()\r\n")
			TEXT("{\r\n")
			TEXT("	vec2 texCoord = vec2(gl_FragCoord.x / 512, -gl_FragCoord.y / 384);\r\n")
			TEXT("	vec4 col = texture2D(image, texCoord);\r\n")
			TEXT("	vec2 p = gl_FragCoord;\r\n")
			TEXT("	float c = 0.0;\r\n")
			TEXT("	for (float i = 0.0; i < 5.0; i++)\r\n")
			TEXT("	{\r\n")
			TEXT("		vec2 b = vec2(\r\n")
			TEXT("		sin(time + i * pi / 7) * 128 + 256,\r\n")
			TEXT("		cos(time + i * pi / 2) * 128 + 192\r\n")
			TEXT("		);\r\n")
			TEXT("		c += 16 / distance(p, b);\r\n")
			TEXT("	}\r\n")
			TEXT("	gl_FragColor = col + c;\r\n")
			TEXT("}\r\n")
			);
		DragAcceptFiles(hWnd, TRUE);
		PostMessage(hWnd, WM_CREATED, 0, 0);
		break;
	case WM_CREATED:
		SendMessage(hWnd, WM_COMMAND, MAKEWPARAM(0, EN_CHANGE), (long)hEdit);
		SendMessage(hEdit, EM_SETEVENTMASK, 0, (LPARAM)(SendMessage(hEdit, EM_GETEVENTMASK, 0, 0) | ENM_CHANGE));
		SetFocus(hEdit);
		break;
	case WM_DROPFILES:
		{
			const HDROP hDrop = (HDROP)wParam;
			TCHAR szFilePath[MAX_PATH];
			DragQueryFile(hDrop, 0, szFilePath, sizeof(szFilePath));
			LPCTSTR lpExt = PathFindExtension(szFilePath);
			if (texture)
			{
				glDeleteTextures(1, &texture);
				texture = 0;
			}
			if (
				PathMatchSpec(lpExt, TEXT("*.jpg")) ||
				PathMatchSpec(lpExt, TEXT("*.jpeg")) ||
				PathMatchSpec(lpExt, TEXT("*.gif")) ||
				PathMatchSpec(lpExt, TEXT("*.png")) ||
				PathMatchSpec(lpExt, TEXT("*.bmp")) ||
				PathMatchSpec(lpExt, TEXT("*.tiff")) ||
				PathMatchSpec(lpExt, TEXT("*.tif"))
				)
			{
				texture = LoadImage(szFilePath);
			}
			DragFinish(hDrop);
		}
		break;
	case WM_SIZE:
		MoveWindow(hEdit, PREVIEW_WIDTH + 20, 10, LOWORD(lParam) - PREVIEW_WIDTH - 30, HIWORD(lParam) - 20, 1);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case 0:
			if (HIWORD(wParam) == EN_CHANGE)
			{
				const DWORD dwSize = GetWindowTextLengthA(hEdit);
				if (!dwSize) return 0;
				LPSTR lpszText = (LPSTR)GlobalAlloc(0, (dwSize + 1)*sizeof(CHAR));
				if (!lpszText) return 0;
				GetWindowTextA(hEdit, lpszText, dwSize + 1);
				const GLuint newProgram = CreateProgram(vsrc, lpszText);
				if (newProgram)
				{
					if (program) glDeleteProgram(program);
					program = newProgram;
					SetWindowText(hWnd, TEXT("フラグメントシェーダ [コンパイル成功]"));
				}
				else
				{
					SetWindowText(hWnd, TEXT("フラグメントシェーダ [コンパイル失敗]"));
				}
				GlobalFree(lpszText);
			}
			break;
		case ID_SELECTALL:
			if (IsWindowVisible(hEdit))
			{
				SendMessage(hEdit, EM_SETSEL, 0, -1);
				SetFocus(hEdit);
			}
			break;
		case 100:
		{
			TCHAR szFileName[MAX_PATH] = { 0 };
			OPENFILENAME ofn;
			ZeroMemory(&ofn, sizeof(ofn));
			ofn.lStructSize = sizeof(OPENFILENAME);
			ofn.hwndOwner = hWnd;
			ofn.lpstrFilter = TEXT("MP4(*.mp4)\0*.mp4\0すべてのファイル(*.*)\0*.*\0\0");
			ofn.lpstrFile = szFileName;
			ofn.nMaxFile = sizeof(szFileName);
			ofn.Flags = OFN_FILEMUSTEXIST | OFN_OVERWRITEPROMPT;
			if (GetSaveFileName(&ofn))
			{
				const DWORD dwOldErrorMode = SetErrorMode(SEM_FAILCRITICALERRORS);
				TCHAR szTempDirectoryPath[MAX_PATH];
				if (CreateTempDirectory(szTempDirectoryPath))
				{
					TCHAR szFFMpegFilePath[MAX_PATH];
					lstrcpy(szFFMpegFilePath, szTempDirectoryPath);
					PathAppend(szFFMpegFilePath, TEXT("FFMPEG.EXE"));
					if (CreateFileFromResource(MAKEINTRESOURCE(IDR_FFMPEG1), TEXT("FFMPEG"), szFFMpegFilePath))
					{
						TCHAR szH264FilePath[MAX_PATH];
						lstrcpy(szH264FilePath, szTempDirectoryPath);
						PathAppend(szH264FilePath, TEXT("FILE.H264"));
						if (CreateH264(szH264FilePath, 10)) // 今は固定で10秒出力
						{
							OutputMP4fromH264(szFFMpegFilePath, szH264FilePath, szFileName);
							DeleteFile(szH264FilePath);
						}
					}
					DeleteFile(szFFMpegFilePath);
					RemoveDirectory(szTempDirectoryPath);
				}
				SetErrorMode(dwOldErrorMode);
				MessageBox(hWnd, TEXT("完了しました。"), TEXT("確認"), MB_ICONINFORMATION);
			}
		}
		break;
		}
		break;
	case WM_ACTIVATE:
		active = !HIWORD(wParam);
		break;
	case WM_DESTROY:
		if (texture)
		{
			glDeleteTextures(1, &texture);
			texture = 0;
		}
		if (program) glDeleteProgram(program);
		if (vbo) glDeleteBuffers(1, &vbo);
		if (vao) glDeleteVertexArrays(1, &vao);
		if (hRC)
		{
			wglMakeCurrent(0, 0);
			wglDeleteContext(hRC);
		}
		if (hDC) ReleaseDC(hStatic, hDC);
		FreeLibrary(hRtLib);
		DeleteObject(hFont);
		PostQuitMessage(0);
		break;
	case WM_SYSCOMMAND:
		switch (wParam)
		{
		case SC_SCREENSAVE:
		case SC_MONITORPOWER:
			return 0;
		}
	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPreInst, LPSTR pCmdLine, int nCmdShow)
{
	ULONG_PTR gdiToken;
	Gdiplus::GdiplusStartupInput gdiSI;
	Gdiplus::GdiplusStartup(&gdiToken, &gdiSI, NULL);
	MSG msg;
	const WNDCLASS wndclass = { 0, WndProc, 0, 0, hInstance, 0, LoadCursor(0, IDC_ARROW), (HBRUSH)(COLOR_WINDOW + 1), 0, szClassName };
	RegisterClass(&wndclass);
	const HWND hWnd = CreateWindow(szClassName, 0, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, 0, 0, hInstance, 0);
	ShowWindow(hWnd, SW_SHOWDEFAULT);
	UpdateWindow(hWnd);
	ACCEL Accel[] = { { FVIRTKEY | FCONTROL, 'A', ID_SELECTALL } };
	const HACCEL hAccel = CreateAcceleratorTable(Accel, sizeof(Accel) / sizeof(ACCEL));
	BOOL done = 0;
	while (!done)
	{
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				done = TRUE;
			}
			else if (!TranslateAccelerator(hWnd, hAccel, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
		else if (active)
		{
			DrawGLScene();
		}
	}
	DestroyAcceleratorTable(hAccel);
	Gdiplus::GdiplusShutdown(gdiToken);
	return msg.wParam;
}
