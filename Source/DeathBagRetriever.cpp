#include "API/ARK/Ark.h"

#include "Structs.h"

#include "DeathBagRetriever.h"

#include "Utils.h"

#include "Retrieve.h"

//#include "RepairItems.h"

#include "Hooks.h"

#include "Timers.h"

#include "Commands.h"

#include "Reload.h"

#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>

#pragma comment(lib, "AsaApi.lib")

/*
Player Automatic retrieval
	- When player dies it all/specified items will be transfer to chosen storage
		- slash command to a storage and save to database
	- When player dies it will equip and retrieve the items in inventory

Player Manual retrieve
	- slash commands

Capture player initial items before dying
*/

#ifndef DECLARE_HOOK
#define DECLARE_HOOK(name, ret, ...) \
	typedef ret (*name##_t)(__VA_ARGS__); \
	extern name##_t name##_original;
#endif

static std::atomic<bool> g_OnServerReadyRunning(false);
static std::atomic<bool> g_Initialized(false);

// Init thread control
static std::thread g_InitThread;
static std::mutex g_InitMutex;
static std::condition_variable g_InitCv;
static std::atomic<bool> g_StopInitThread(false);
static std::atomic<bool> g_PendingInit(false);

void OnServerReady()
{
	// Guard gegen gleichzeitigen Aufruf
	bool expected = false;
	if (!g_OnServerReadyRunning.compare_exchange_strong(expected, true))
	{
		// nur Fehler-Logs behalten -> kein Log-Ausdruck hier
		return;
	}

	try
	{
		if (!g_Initialized.load())
		{
			ReadConfig();

			// LoadDatabase optional
			try
			{
				LoadDatabase();
			}
			catch (const std::exception& e)
			{
				Log::GetLog()->error("OnServerReady: LoadDatabase std::exception: {}", e.what());
			}
			catch (...)
			{
				Log::GetLog()->error("OnServerReady: LoadDatabase unknown exception");
			}

			g_Initialized.store(true);
		}

		// Registrierung/Setup
		try
		{
			AddOrRemoveCommands();
		}
		catch (const std::exception& e)
		{
			Log::GetLog()->error("OnServerReady: AddOrRemoveCommands std::exception: {}", e.what());
		}
		catch (...)
		{
			Log::GetLog()->error("OnServerReady: AddOrRemoveCommands unknown exception");
		}

		try
		{
			SetTimers();
		}
		catch (const std::exception& e)
		{
			Log::GetLog()->error("OnServerReady: SetTimers std::exception: {}", e.what());
		}
		catch (...)
		{
			Log::GetLog()->error("OnServerReady: SetTimers unknown exception");
		}

		try
		{
			SetHooks();
		}
		catch (const std::exception& e)
		{
			Log::GetLog()->error("OnServerReady: SetHooks std::exception: {}", e.what());
		}
		catch (...)
		{
			Log::GetLog()->error("OnServerReady: SetHooks unknown exception");
		}
	}
	catch (const std::exception& ex)
	{
		Log::GetLog()->error("OnServerReady: std::exception: {}", ex.what());
	}
	catch (...)
	{
		Log::GetLog()->error("OnServerReady: unknown exception");
	}

	g_OnServerReadyRunning.store(false);
}

DECLARE_HOOK(AShooterGameMode_BeginPlay, void, AShooterGameMode*);
void Hook_AShooterGameMode_BeginPlay(AShooterGameMode* _this)
{
	// Hook möglichst leicht halten: original aufrufen und Init signalisieren.
	try
	{
		if (AShooterGameMode_BeginPlay_original)
		{
			try
			{
				AShooterGameMode_BeginPlay_original(_this);
			}
			catch (const std::exception& e)
			{
				Log::GetLog()->error("Hook_AShooterGameMode_BeginPlay: original threw std::exception: {}", e.what());
			}
			catch (...)
			{
				Log::GetLog()->error("Hook_AShooterGameMode_BeginPlay: original threw unknown exception");
			}
		}
	}
	catch (...)
	{
		Log::GetLog()->error("Hook_AShooterGameMode_BeginPlay: error while calling original");
	}

	// Signal für Hintergrund-Init (keine direkte Initialisierung im Hook)
	g_PendingInit.store(true);
	g_InitCv.notify_one();
}

static void InitThreadProc()
{
	while (!g_StopInitThread.load())
	{
		std::unique_lock<std::mutex> lk(g_InitMutex);
		g_InitCv.wait_for(lk, std::chrono::milliseconds(500), [] { return g_PendingInit.load() || g_StopInitThread.load(); });

		if (g_StopInitThread.load())
			break;

		if (g_PendingInit.exchange(false))
		{
			// kurze Verzögerung damit Engine/BeginPlay vollständig durchlaufen kann
			std::this_thread::sleep_for(std::chrono::milliseconds(100));

			try
			{
				// VORSICHT: manche Engine-APIs erfordern Main-Thread-Aufrufe.
				OnServerReady();
			}
			catch (const std::exception& e)
			{
				Log::GetLog()->error("InitThreadProc: OnServerReady std::exception: {}", e.what());
			}
			catch (...)
			{
				Log::GetLog()->error("InitThreadProc: OnServerReady unknown exception");
			}
		}
	}
}

extern "C" __declspec(dllexport) void Plugin_Init()
{
	// Start Init-Thread einmalig
	if (!g_InitThread.joinable())
	{
		g_StopInitThread.store(false);
		g_InitThread = std::thread(InitThreadProc);
	}

	// Logging-Init (keine Info-Logs)
	try
	{
		Log::Get().Init(PROJECT_NAME);
	}
	catch (const std::exception& e)
	{
		Log::GetLog()->error("Plugin_Init: Log init std::exception: {}", e.what());
	}
	catch (...)
	{
		Log::GetLog()->error("Plugin_Init: Log init unknown exception");
	}

	// Register hook
	try
	{
		AsaApi::GetHooks().SetHook("AShooterGameMode.BeginPlay()", Hook_AShooterGameMode_BeginPlay, &AShooterGameMode_BeginPlay_original);
	}
	catch (const std::exception& e)
	{
		Log::GetLog()->error("Plugin_Init: SetHook std::exception: {}", e.what());
	}
	catch (...)
	{
		Log::GetLog()->error("Plugin_Init: SetHook unknown exception");
	}

	// Falls Server bereits Ready ist, schedule Init sofort
	try
	{
		if (AsaApi::GetApiUtils().GetStatus() == AsaApi::ServerStatus::Ready)
		{
			g_PendingInit.store(true);
			g_InitCv.notify_one();
		}
	}
	catch (const std::exception& e)
	{
		Log::GetLog()->error("Plugin_Init: checking server status std::exception: {}", e.what());
	}
	catch (...)
	{
		Log::GetLog()->error("Plugin_Init: checking server status unknown exception");
	}
}

extern "C" __declspec(dllexport) void Plugin_Unload()
{
	// Stop Init-Thread sauber
	g_StopInitThread.store(true);
	g_InitCv.notify_one();
	if (g_InitThread.joinable())
		g_InitThread.join();

	// Disable hook & cleanup
	try
	{
		AsaApi::GetHooks().DisableHook("AShooterGameMode.BeginPlay()", Hook_AShooterGameMode_BeginPlay);
	}
	catch (const std::exception& e)
	{
		Log::GetLog()->error("Plugin_Unload: DisableHook std::exception: {}", e.what());
	}
	catch (...)
	{
		Log::GetLog()->error("Plugin_Unload: DisableHook unknown exception");
	}

	try
	{
		AddOrRemoveCommands(false);
		AddReloadCommands(false);
		SetTimers(false);
		SetHooks(false);
	}
	catch (const std::exception& e)
	{
		Log::GetLog()->error("Plugin_Unload: cleanup std::exception: {}", e.what());
	}
	catch (...)
	{
		Log::GetLog()->error("Plugin_Unload: cleanup unknown exception");
	}
}