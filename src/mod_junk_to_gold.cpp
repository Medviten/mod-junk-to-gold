#include "Chat.h"
#include "ChatCommand.h"
#include "CommandScript.h"
#include "Config.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldScript.h"

#include <algorithm>
#include <optional>
#include <string>

using namespace Acore::ChatCommands;

/*
 * Per-player toggle state is stored with AzerothCore's built-in player settings
 * (persisted in the `character_settings` table), so no extra table is needed.
 *
 *   0 = player never used .trashtocash -> follow JunkToGold.Toggle.DefaultEnabled
 *   1 = player turned auto-sell ON
 *   2 = player turned auto-sell OFF
 */
namespace
{
    constexpr char const* JTG_SETTING_SOURCE = "mod-junk-to-gold";
    constexpr uint8 JTG_SETTING_INDEX = 0;

    enum JunkToGoldState : uint32
    {
        JTG_STATE_DEFAULT = 0,
        JTG_STATE_ON = 1,
        JTG_STATE_OFF = 2
    };

    bool g_defaultEnabled = true;

    bool IsJunkToGoldEnabledFor(Player* player)
    {
        switch (player->GetPlayerSetting(JTG_SETTING_SOURCE, JTG_SETTING_INDEX).value)
        {
            case JTG_STATE_ON:
                return true;
            case JTG_STATE_OFF:
                return false;
            default:
                return g_defaultEnabled;
        }
    }
}

class JunkToGold : public PlayerScript
{
public:
    JunkToGold() : PlayerScript("JunkToGold") {}

    void OnPlayerLootItem(Player* player, Item* item, uint32 count, ObjectGuid /*lootguid*/) override
    {
        if (!item || !item->GetTemplate())
        {
            return;
        }

        if (item->GetTemplate()->Quality == ITEM_QUALITY_POOR)
        {
            if (!IsJunkToGoldEnabledFor(player))
            {
                return;
            }

            SendTransactionInformation(player, item, count);
            player->ModifyMoney(item->GetTemplate()->SellPrice * count);
            player->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        }
    }

private:
    void SendTransactionInformation(Player* player, Item* item, uint32 count)
    {
        std::string name;
        if (count > 1)
        {
            name = Acore::StringFormat("|cff9d9d9d|Hitem:{}::::::::80:::::|h[{}]|h|rx{}", item->GetTemplate()->ItemId, item->GetTemplate()->Name1, count);
        }
        else
        {
            name = Acore::StringFormat("|cff9d9d9d|Hitem:{}::::::::80:::::|h[{}]|h|r", item->GetTemplate()->ItemId, item->GetTemplate()->Name1);
        }

        uint32 money = item->GetTemplate()->SellPrice * count;
        uint32 gold = money / GOLD;
        uint32 silver = (money % GOLD) / SILVER;
        uint32 copper = (money % GOLD) % SILVER;

        std::string info;
        if (money < SILVER)
        {
            info = Acore::StringFormat("{} sold for {} copper.", name, copper);
        }
        else if (money < GOLD)
        {
            if (copper > 0)
            {
                info = Acore::StringFormat("{} sold for {} silver and {} copper.", name, silver, copper);
            }
            else
            {
                info = Acore::StringFormat("{} sold for {} silver.", name, silver);
            }
        }
        else
        {
            if (copper > 0 && silver > 0)
            {
                info = Acore::StringFormat("{} sold for {} gold, {} silver and {} copper.", name, gold, silver, copper);
            }
            else if (copper > 0)
            {
                info = Acore::StringFormat("{} sold for {} gold and {} copper.", name, gold, copper);
            }
            else if (silver > 0)
            {
                info = Acore::StringFormat("{} sold for {} gold and {} silver.", name, gold, silver);
            }
            else
            {
                info = Acore::StringFormat("{} sold for {} gold.", name, gold);
            }
        }

        ChatHandler(player->GetSession()).SendSysMessage(info);
    }
};

class JunkToGoldWorldScript : public WorldScript
{
public:
    JunkToGoldWorldScript() : WorldScript("JunkToGoldWorldScript", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD
    }) { }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_defaultEnabled = sConfigMgr->GetOption<bool>("JunkToGold.Toggle.DefaultEnabled", true, false);
    }
};

class JunkToGoldCommandScript : public CommandScript
{
public:
    JunkToGoldCommandScript() : CommandScript("JunkToGoldCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "trashtocash", HandleTrashToCashCommand, SEC_PLAYER, Console::No }
        };
        return commandTable;
    }

    // .trashtocash            -> toggle
    // .trashtocash on|off     -> set explicitly
    // .trashtocash status     -> report current state
    static bool HandleTrashToCashCommand(ChatHandler* handler, Optional<std::string> argStr)
    {
        Player* player = handler->GetSession() ? handler->GetSession()->GetPlayer() : nullptr;
        if (!player)
        {
            return false;
        }

        bool const currentlyEnabled = IsJunkToGoldEnabledFor(player);

        std::string arg = argStr.value_or("");
        std::transform(arg.begin(), arg.end(), arg.begin(), ::tolower);

        bool newState;
        if (arg == "on")
        {
            newState = true;
        }
        else if (arg == "off")
        {
            newState = false;
        }
        else if (arg == "status")
        {
            handler->PSendSysMessage("Trash to Cash is currently {} for you.", currentlyEnabled ? "|cff00ff00ON|r" : "|cffff0000OFF|r");
            return true;
        }
        else
        {
            newState = !currentlyEnabled; // no/unrecognized arg -> toggle
        }

        player->UpdatePlayerSetting(JTG_SETTING_SOURCE, JTG_SETTING_INDEX, newState ? JTG_STATE_ON : JTG_STATE_OFF);

        if (newState)
        {
            handler->SendSysMessage("Trash to Cash: |cff00ff00ON|r. Gray items are sold automatically when you loot them.");
        }
        else
        {
            handler->SendSysMessage("Trash to Cash: |cffff0000OFF|r. Gray items will stay in your bags.");
        }

        return true;
    }
};

void Addmod_junk_to_goldScripts()
{
    new JunkToGold();
    new JunkToGoldWorldScript();
    new JunkToGoldCommandScript();
}