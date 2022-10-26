		/*  @'   @+          @;         @@@@@         @@@@@,                  @
			@@  ;@+          @;         @,  @         @'  @,                  @           @
			@@, @@+ +@@@: @@@@; @@@@    @,  @ @; '@   @'  @, @@@@  @@@@ :@@@' @@@@, @@@@:@@@#
			@,@.@#+ ,. ## @  @; @  @    @@@@@ ,@ @:   @'  @, @` @` @  @ +@ :; @, @; @  @' @.
			@ @@:#+ '@@@# @  @; @@@@    @,  @  @ @    @'  @, @  @` @@@@  @@@  @  @; @  @' @.
			@ '@ #+ @. @# @  @; @  .    @,  @  '@     @;  @, @  @` @  . `` +@ @  @; @  @' @.
			@    #+ @@@@# @@@@; @@@@    @@@@@ :@:     @@@@@. @  @. @@@@ '@@@@ @` @; @@@@' @@+ */
#include <Windows.h>
#include "detours.h"
#include "Patternscaning.h"
#include <iostream>

#include <d3d9.h>
#include <d3dx9.h>
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

#include "../ImGui/imgui.h"
#include "../ImGui/imgui_impl_dx9.h"
#include "../ImGui/imgui_impl_win32.h"


const char* windowName = "Counter-Strike Source"; 

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

typedef HRESULT(__stdcall * f_EndScene)(IDirect3DDevice9 * pDevice); // Our function prototype 
f_EndScene oEndScene; // Original Endscene

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);

#define dwClientState 0x05AAAA4
#define dwClientState_ViewMatrix 0x2D4

#define dwLocalPlayer 0x04C88E8
#define dwEntityList 0x04D5AF4

#define m_iHealth 0x94
#define m_iTeamNum 0x9C
#define m_bDormant 0x17C
#define m_vecOrigin 0x260
#define m_fVelocity 0x5D4
#define m_fFlags 0x350
#define m_iAirState 0x5DC
#define dwForceJump 0x4F5D24

struct Vector
{
public:
	float x, y, z, w;

	Vector()
	{
	}

	Vector(float X, float Y)
	{
		x = X;
		y = Y;
	}

	Vector(float X, float Y, float Z)
	{
		x = X;
		y = Y;
		z = Z;
	}

	Vector(float X, float Y, float Z, float W)
	{
		x = X;
		y = Y;
		z = Z;
		w = W;
	}

	inline Vector operator+(Vector dst)
	{
		return { x + dst.x, y + dst.y, z + dst.z, w + dst.w };
	}

	inline Vector operator-(Vector dst)
	{
		return { x - dst.x, y - dst.y, z - dst.z, w - dst.w};
	}
};

struct ViewMatrix_t
{
	float matrix[4][4];
};

void move_to(float x, float y, float smooth, int screenWidth, float screenHeight)
{
	float center_x = screenWidth / 2;
	float center_y = screenHeight / 2;

	float target_x = 0.f;
	float target_y = 0.f;

	if (x != 0.f)
	{
		if (x > center_x)
		{
			target_x = -(center_x - x);
			target_x /= smooth;
			if (target_x + center_x > center_x * 2.f) target_x = 0.f;
		}

		if (x < center_x)
		{
			target_x = x - center_x;
			target_x /= smooth;
			if (target_x + center_x < 0.f) target_x = 0.f;
		}
	}

	if (y != 0.f)
	{
		if (y > center_y)
		{
			target_y = -(center_y - y);
			target_y /= smooth;
			if (target_y + center_y > center_y * 2.f) target_y = 0.f;
		}

		if (y < center_y)
		{
			target_y = y - center_y;
			target_y /= smooth;
			if (target_y + center_y < 0.f) target_y = 0.f;
		}
	}

	mouse_event(MOUSEEVENTF_MOVE, static_cast<DWORD>(target_x), static_cast<DWORD>(target_y), 0, 0);
}

static bool WorldToScreen(Vector origin, Vector& screen, ViewMatrix_t view)
{
	screen = Vector(0, 0);
	Vector ClipCoords = Vector();
	ClipCoords.x = origin.x * view.matrix[0][0] + origin.y * view.matrix[0][1] + origin.z * view.matrix[0][2] + view.matrix[0][3];
	ClipCoords.y = origin.x * view.matrix[1][0] + origin.y * view.matrix[1][1] + origin.z * view.matrix[1][2] + view.matrix[1][3];
	ClipCoords.z = origin.x * view.matrix[2][0] + origin.y * view.matrix[2][1] + origin.z * view.matrix[2][2] + view.matrix[2][3];
	ClipCoords.w = origin.x * view.matrix[3][0] + origin.y * view.matrix[3][1] + origin.z * view.matrix[3][2] + view.matrix[3][3];

	if (ClipCoords.w < 0.1f)
		return false;

	Vector NDC;
	NDC.x = ClipCoords.x / ClipCoords.w;
	NDC.y = ClipCoords.y / ClipCoords.w;
	NDC.z = ClipCoords.z / ClipCoords.w;

	int GameWidth = ImGui::GetIO().DisplaySize.x;
	int GameHeight = ImGui::GetIO().DisplaySize.y;
	screen.x = (GameWidth / 2 * NDC.x) + (NDC.x + GameWidth / 2);
	screen.y = -(GameHeight / 2 * NDC.y) + (NDC.y + GameHeight / 2);
	return true;
}

float DistanceCalc2D(Vector start, Vector end)
{
	return sqrt((pow(start.x - end.x, 2) + pow(start.y - end.y, 2)));
}

static bool ESP = false;
static bool Chams = false;
static bool HealthESP = false;
static bool Tracers = false;

static float AimbotFOV = 120.f;
static float AimbotSens = 1.f;
static bool Aimbot = false;
static bool DrawFOV = false;

static bool ThirdPerson = false;
static bool BunnyHop = false;
static bool NoFlash = false;

static DWORD Client;
static DWORD Engine;
static void Cheat()
{
	float GameWidth = ImGui::GetIO().DisplaySize.x;
	float GameHeight = ImGui::GetIO().DisplaySize.y;
	Vector GameCenter = Vector(GameWidth / 2, GameHeight / 2);
	
	uintptr_t LocalPlayer = *(uintptr_t*)(Client + dwLocalPlayer);
	if (!LocalPlayer)
		return;

	if (DrawFOV)
		ImGui::GetOverlayDrawList()->AddCircle({ GameCenter.x, GameCenter.y }, AimbotFOV, IM_COL32_WHITE, 60);

	float Velocity = abs(*(float*)(LocalPlayer + 0xF4));
	int LocalTeam = *(int*)(LocalPlayer + m_iTeamNum);
	int Flags = *(int*)(LocalPlayer + m_fFlags);
	int LocalHealth = *(int*)(LocalPlayer + m_iHealth);

	uintptr_t ClientState = *(uintptr_t*)(Engine + dwClientState);
	ViewMatrix_t ViewMatrix = *(ViewMatrix_t*)(ClientState + dwClientState_ViewMatrix);

	float FlashMaxAlpha = *(float*)(LocalPlayer + 0x1450);
	int ObserverMode = *(int*)(LocalPlayer + 0x103C);

	uintptr_t TargetPlayer = -1;
	float MAX_DIST = FLT_MAX;
	for (int i = 0; i < 32; i++)
	{
		uintptr_t Entity = *(uintptr_t*)(Client + dwEntityList + (i * 0x10));
		if (!Entity)
			continue;

		int Dormant = *(int*)(Entity + m_bDormant);
		if (Dormant != 0x0) // maybe doesnt works, i'm not sure about that
			continue;

		int Health = *(int*)(Entity + m_iHealth);
		if (!Health)
			continue;

		int LifeState = *(int*)(Entity + 0x93);
		if (LifeState <= 0x101) // is alive check
			continue;

		int Team = *(int*)(Entity + m_iTeamNum);

		ImColor TracersColor = (LocalTeam == Team) ? ImColor(0, 0, 255, 255) : ImColor(255, 0, 0, 255);
		byte colors[3] = { 255, 0, 0 };
		colors[0] = (LocalTeam == Team) ? 0 : 255;
		colors[2] = (LocalTeam == Team) ? 255 : 0;

		// CHAMS
		if (Chams)
		{
			*(byte*)(Entity + 0x5C) = colors[0];
			*(byte*)(Entity + 0x5C + 0x1) = colors[1];
			*(byte*)(Entity + 0x5C + (0x1 + 0x1)) = colors[2];
		}
		else
		{
			*(byte*)(Entity + 0x5C) = byte(255);
			*(byte*)(Entity + 0x5C + 0x1) = byte(255);
			*(byte*)(Entity + 0x5C + (0x1 + 0x1)) = byte(255);
		}

		Vector Entity2D;
		Vector EntityOrigin = *(Vector*)(Entity + m_vecOrigin);
		if (WorldToScreen(EntityOrigin, Entity2D, ViewMatrix))
		{
			// SNAPLINES
			if (Tracers)
			{
				ImGui::GetOverlayDrawList()->AddLine({ GameWidth / 2, GameHeight }, { Entity2D.x, Entity2D.y }, IM_COL32_BLACK, 3.f);
				ImGui::GetOverlayDrawList()->AddLine({ GameWidth / 2, GameHeight }, { Entity2D.x, Entity2D.y }, TracersColor);
			}

			Vector VecMax2D, VecMin2D;
			Vector VecMax = *(Vector*)(Entity + 0x19C + 0x2C);
			Vector VecMin = *(Vector*)(Entity + 0x19C + 0x20);

			if (WorldToScreen(VecMax + EntityOrigin, VecMax2D, ViewMatrix) && WorldToScreen(VecMin + EntityOrigin, VecMin2D, ViewMatrix))
			{
				float BoxHeight = VecMin2D.y - VecMax2D.y;
				float BoxWidth = BoxHeight / 2.4f;

				float HeadPosX = VecMax2D.x + (VecMin2D.x - VecMax2D.x) / 2;
				float x1 = HeadPosX - (BoxWidth / 2.f);
				float y1 = VecMax2D.y;

				// ESP
				if (ESP)
				{
					ImGui::GetOverlayDrawList()->AddRect({ x1, y1 }, { x1 + BoxWidth, y1 + BoxHeight }, IM_COL32_BLACK, 0.f, 15, 3.f);
					ImGui::GetOverlayDrawList()->AddRect({ x1, y1 }, { x1 + BoxWidth, y1 + BoxHeight }, TracersColor);

					if (Health)
					{
						ImGui::GetOverlayDrawList()->AddRectFilled({ x1 + BoxWidth, y1 + BoxHeight }, { x1 + BoxWidth + BoxWidth / 6, y1 + BoxHeight }, ImColor(0, 255, 0, 255));
						ImGui::GetOverlayDrawList()->AddRect({ x1 + BoxWidth, y1 + BoxHeight }, { x1 + BoxWidth + BoxWidth / 6, y1 + BoxHeight }, IM_COL32_BLACK);
					}
				}
	/*			ImGui::GetOverlayDrawList()->AddRect({ VecMax2D.x, VecMax2D.y }, { VecMin2D.x, VecMin2D.y }, IM_COL32_WHITE);*/

				if (LocalTeam != Team)
				{
					float CUR_DIST = DistanceCalc2D(GameCenter, { HeadPosX, VecMax2D.y });
					if (CUR_DIST <= AimbotFOV)
					{
						if (MAX_DIST > CUR_DIST)
						{
							TargetPlayer = Entity;
							MAX_DIST = CUR_DIST;
						}
					}
				}
			}
		}
	}

	// AIMBOT
	if (Aimbot)
	{
		if (GetAsyncKeyState(VK_XBUTTON2) & 0x8000)
		{
			if (TargetPlayer != -1)
			{
				Vector Entity2D;
				Vector EntityOrigin = *(Vector*)(TargetPlayer + m_vecOrigin);

				Vector VecMax2D, VecMin2D;
				Vector max = *(Vector*)(TargetPlayer + 0x19C + 0x2C) + EntityOrigin;
				Vector min = *(Vector*)(TargetPlayer + 0x19C + 0x20) + EntityOrigin;

				if (WorldToScreen(max, VecMax2D, ViewMatrix) && WorldToScreen(min, VecMin2D, ViewMatrix))
				{
					float HeadPosX = VecMax2D.x + (VecMin2D.x - VecMax2D.x) / 2;
					move_to(HeadPosX, VecMax2D.y, AimbotSens, GameWidth, GameHeight);
				}
			}
		}
	}

	// BUNNY HOP
	if (BunnyHop)
	{
		if ((GetAsyncKeyState(VK_SPACE) & 0x8000) && Velocity >= 1.f)
		{
			if (Flags == 0x101 || Flags == 0x107)
			{
				*(int*)(Client + dwForceJump) = 6;
			}
		}
	}

	// 3RD PERSON
	if (ThirdPerson)
	{
		if (LocalHealth > 0)
		{
			if (GetAsyncKeyState(VK_XBUTTON1) & 0x8000)
			{
				*(int*)(LocalPlayer + 0x103C) = 1;
			}
			else
			{
				if (ObserverMode == 1)
					*(int*)(LocalPlayer + 0x103C) = 0;
			}
		}
	}

	// NO FLASH
	if (NoFlash)
	{
		if (FlashMaxAlpha > 0.f)
			*(float*)(LocalPlayer + 0x1450) = 0.f;
	}
}

static bool ShowMenu = false;
WNDPROC oWndProc;
HRESULT __stdcall Hooked_EndScene(IDirect3DDevice9 * pDevice) // Our hooked endscene
{
	static bool init = true;
	if (init)
	{
		init = false;
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		
		ImGui_ImplWin32_Init(FindWindowA(NULL, windowName));
		ImGui_ImplDX9_Init(pDevice);

		while (!GetModuleHandleA("ServerBrowser")) { Sleep(500); }
		Client = (DWORD)GetModuleHandleA("client.dll");
		Engine = (DWORD)GetModuleHandleA("engine.dll");
		std::cout << "Client = 0x" << std::hex << Client << std::endl;
		std::cout << "Engine = 0x" << std::hex << Engine << std::endl;
	}

	if (GetAsyncKeyState(VK_INSERT) & 1)
		ShowMenu = !ShowMenu;
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame(); // START RENDER

	static int tabIndex = 0;
	if (ShowMenu)
	{
		ImGui::Begin("CSS - absent");
		if (ImGui::Button("Visuals")) { tabIndex = 0; }
		ImGui::SameLine();
		if (ImGui::Button("Aimbot")) { tabIndex = 1; }
		ImGui::SameLine();
		if (ImGui::Button("Miscs")) { tabIndex = 2; }

		if (tabIndex == 0)
		{
			ImGui::Checkbox("ESP", &ESP);
			ImGui::Checkbox("Tracers", &Tracers);
			ImGui::Checkbox("Health", &HealthESP);
			ImGui::Checkbox("Chams", &Chams);
		}
		else if (tabIndex == 1)
		{
			ImGui::Checkbox("Aimbot ", &Aimbot);
			ImGui::SameLine();
			ImGui::Checkbox("Draw FOV", &DrawFOV);
			ImGui::SliderFloat("FOV", &AimbotFOV, 0.f, 360.f);
			ImGui::SliderFloat("Sensivity", &AimbotSens, 1.f, 10.f);
		}
		else if (tabIndex == 2)
		{
			ImGui::Checkbox("Auto Bunny", &BunnyHop);
			ImGui::Checkbox("3rd Person", &ThirdPerson);
			ImGui::Checkbox("No Flash", &NoFlash);
		}
		ImGui::End();
	}

	Cheat(); // RENDER CHEAT
	ImGui::EndFrame(); // END RENDER
	ImGui::Render();

	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
	
	return oEndScene(pDevice); // Call original ensdcene so the game can draw
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {

	if (true && ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
		return true;

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}


DWORD WINAPI MainThread(LPVOID param) // Our main thread
{
	HWND  window = FindWindowA(NULL, windowName);

	oWndProc = (WNDPROC)SetWindowLongPtr(window, GWL_WNDPROC, (LONG_PTR)WndProc);

	IDirect3D9 * pD3D = Direct3DCreate9(D3D_SDK_VERSION);

	if (!pD3D)
		return false;

	D3DPRESENT_PARAMETERS d3dpp{ 0 };
	d3dpp.hDeviceWindow = window, d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD , d3dpp.Windowed = TRUE;
	
	IDirect3DDevice9 *Device = nullptr;
	if (FAILED(pD3D->CreateDevice(0, D3DDEVTYPE_HAL, d3dpp.hDeviceWindow, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &Device)))
	{
		pD3D->Release();
		return false;
	}
		

	void ** pVTable = *reinterpret_cast<void***>(Device); 

	if (Device)
		Device->Release() , Device = nullptr;
		
	oEndScene = (f_EndScene)DetourFunction((PBYTE)pVTable[42], (PBYTE)Hooked_EndScene); 
	
	return false; 
}


BOOL APIENTRY DllMain(HMODULE hModule,DWORD  ul_reason_for_call,LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH: // Gets runned when injected
		AllocConsole(); // Enables the console
		freopen("CONIN$", "r", stdin); // Makes it possible to output to output to console with cout.
		freopen("CONOUT$", "w", stdout);
		CreateThread(0, 0, MainThread, hModule, 0, 0); // Creates our thread 
		break;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}

