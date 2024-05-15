﻿#include "hooks.h"
#include "globals.h"

hooks::hooks()
{
    hook_script("scr_pattern_deal_damage_enemy_subtract",
                [](YYTK::CInstance* self, YYTK::CInstance* other,
                   YYTK::RValue& return_value, int num_args, YYTK::RValue** args,
                   ScriptFunction* original)
                {
                    if (g_config.cheat_damage) args[2]->m_Real = 42069;
                    return_value = original(self, other, return_value, num_args, args);
                });
}

static int current_hook_id = 0;
static int hook_index = 0;

YYTK::RValue& detour_func(YYTK::CInstance* self, YYTK::CInstance* other, YYTK::RValue& return_value,
                          int num_args, YYTK::RValue** args)
{
    auto original = hooks::original_map[current_hook_id];

    if (hooks::detour_map.contains(current_hook_id))
    {
        hooks::detour_map[current_hook_id](self, other, return_value, num_args, args, original);
    }
    else
    {
        original(self, other, return_value, num_args, args);
    }

    return return_value;
}

int hooks::hook_script(
    std::string_view script_name,
    ScriptDetour callback
)
{
    auto runner = g_module_interface->GetRunnerInterface();
    auto id = runner.Script_Find_Id(script_name.data()) - 100000;

    CScript* script = nullptr;
    auto status = g_module_interface->GetScriptData(id, script);
    if (!AurieSuccess(status) || script == nullptr)
    {
        g_module_interface->Print(CM_LIGHTRED, "Failed to get script data for %s", script_name.data());
        return -1;
    }

    auto code = asmjit::CodeHolder();
    code.init(runtime.environment(), runtime.cpuFeatures());
    auto assembler = asmjit::x86::Assembler(&code);

    auto hook_id = hook_index;

    assembler.mov(asmjit::x86::ptr(current_hook_id), hook_id);
    assembler.jmp(detour_func);

    GeneratedDetourFunction detour;
    runtime.add(&detour, &code);

    detour_map[hook_id] = callback;

    void* original = nullptr;
    MmCreateHook(g_module,
                 "Bouny-" + std::to_string(hook_id),
                 script->m_Functions->m_ScriptFunction,
                 detour,
                 &original);
    original_map[hook_id] = reinterpret_cast<ScriptFunction*>(original);

    hook_index++;
    return hook_id;
}

void hooks::unhook_script(int id)
{
    // TODO: Doesn't actually remove the allocated function from asmjit...
    auto script_name = "Bouny-" + std::to_string(id);
    MmRemoveHook(g_module, script_name);
}
