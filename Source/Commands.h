void AddOrRemoveCommands(bool addCmd = true)
{
	try
	{
		// Schütze gegen fehlende/NULL-Einträge in der Config und temporäre .c_str()-Pointer
		const nlohmann::json cmds = DeathBagRetriever::config.value("Commands", nlohmann::json::object());

		{
			std::string checkDebugStr = cmds.value("CheckCMD", "");
			if (!checkDebugStr.empty())
			{
				FString CheckDebug = checkDebugStr.c_str();
				try
				{
					if (addCmd)
						AsaApi::GetCommands().AddChatCommand(CheckDebug, &CheckCallback);
					else
						AsaApi::GetCommands().RemoveChatCommand(CheckDebug);
				}
				catch (const std::exception& e)
				{
					Log::GetLog()->error("AddOrRemoveCommands: CheckCMD '{}' failed: {}", checkDebugStr, e.what());
				}
				catch (...)
				{
					Log::GetLog()->error("AddOrRemoveCommands: CheckCMD '{}' failed: unknown error", checkDebugStr);
				}
			}
		}

		{
			std::string killMeStr = cmds.value("KillMeCMD", "");
			if (!killMeStr.empty())
			{
				FString KillMe = killMeStr.c_str();
				try
				{
					if (addCmd)
						AsaApi::GetCommands().AddChatCommand(KillMe, &KillMeCallBack);
					else
						AsaApi::GetCommands().RemoveChatCommand(KillMe);
				}
				catch (const std::exception& e)
				{
					Log::GetLog()->error("AddOrRemoveCommands: KillMeCMD '{}' failed: {}", killMeStr, e.what());
				}
				catch (...)
				{
					Log::GetLog()->error("AddOrRemoveCommands: KillMeCMD '{}' failed: unknown error", killMeStr);
				}
			}
		}

		{
			std::string teleportStr = cmds.value("TeleportCMD", "");
			if (!teleportStr.empty())
			{
				FString Teleport = teleportStr.c_str();
				try
				{
					if (addCmd)
						AsaApi::GetCommands().AddChatCommand(Teleport, &TeleportCallBack);
					else
						AsaApi::GetCommands().RemoveChatCommand(Teleport);
				}
				catch (const std::exception& e)
				{
					Log::GetLog()->error("AddOrRemoveCommands: TeleportCMD '{}' failed: {}", teleportStr, e.what());
				}
				catch (...)
				{
					Log::GetLog()->error("AddOrRemoveCommands: TeleportCMD '{}' failed: unknown error", teleportStr);
				}
			}
		}

		// Auskommentierte/zusätzliche Commands belassen wie im Original, ggf. analog anpassen wenn aktiviert.
	}
	catch (const std::exception& e)
	{
		Log::GetLog()->error("AddOrRemoveCommands: std::exception: {}", e.what());
	}
	catch (...)
	{
		Log::GetLog()->error("AddOrRemoveCommands: unknown exception");
	}
}