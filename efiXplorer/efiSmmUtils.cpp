/*
 * efiXplorer
 * Copyright (C) 2020-2021 Binarly
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * efiSmmUtils.cpp
 *
 */

#include "efiSmmUtils.h"

static const char plugin_name[] = "efiXplorer";

//--------------------------------------------------------------------------
// Find and mark gSmst global variable via EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID
std::vector<ea_t> findSmstSwDispatch(std::vector<ea_t> gBsList,
                                     std::vector<segment_t *> dataSegments) {
    std::vector<ea_t> resAddrs;
    efiGuid efiSmmSwDispatch2ProtocolGuid = {
        0x18a3c6dc, 0x5eea, 0x48c8, {0xa1, 0xc1, 0xb5, 0x33, 0x89, 0xf9, 0x89, 0x99}};
    efiGuid efiSmmSwDispatchProtocolGuid = {
        0xe541b773, 0xdd11, 0x420c, {0xb0, 0x26, 0xdf, 0x99, 0x36, 0x53, 0xf8, 0xbf}};
    ea_t efiSmmSwDispatchProtocolGuidAddr = 0;

    for (auto seg_info : dataSegments) {
        if (seg_info == nullptr) {
            return resAddrs;
        }
        DEBUG_MSG("[%s] gSmst finding from 0x%016X to 0x%016X (via "
                  "EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID)\n",
                  plugin_name, seg_info->start_ea, seg_info->end_ea);
        ea_t ea = seg_info->start_ea;
        while (ea != BADADDR && ea <= seg_info->end_ea - 15) {
            if (get_wide_dword(ea) == efiSmmSwDispatchProtocolGuid.data1) {
                efiSmmSwDispatchProtocolGuidAddr = ea;
                break;
            }
            ea += 1;
        }

        if (!efiSmmSwDispatchProtocolGuidAddr) {
            continue;
        }

        std::vector<ea_t> efiSmmSwDispatchProtocolGuidXrefs =
            getXrefs(efiSmmSwDispatchProtocolGuidAddr);

        for (auto guidXref : efiSmmSwDispatchProtocolGuidXrefs) {
            ea_t curAddr = prev_head(static_cast<ea_t>(guidXref), 0);
            ea_t resAddr = 0;
            insn_t insn;

            // Check 4 instructions below
            for (auto i = 0; i < 4; i++) {
                decode_insn(&insn, curAddr);
                if (insn.itype == NN_mov && insn.ops[0].type == o_reg &&
                    insn.ops[0].reg == REG_RAX && insn.ops[1].type == o_mem) {
                    DEBUG_MSG("[%s] found gSmst at 0x%016X, address = 0x%016X\n",
                              plugin_name, curAddr, insn.ops[1].addr);
                    resAddr = insn.ops[1].addr;
                    if (find(gBsList.begin(), gBsList.end(), resAddr) != gBsList.end()) {
                        continue;
                    }
                    set_cmt(curAddr, "_EFI_SMM_SYSTEM_TABLE2 *gSmst;", true);
                    std::string hexstr = getHex(static_cast<uint64_t>(resAddr));
                    std::string name = "gSmst_" + hexstr;
                    setPtrTypeAndName(resAddr, name, "_EFI_SMM_SYSTEM_TABLE2");
                    resAddrs.push_back(resAddr);
                    break;
                }
                curAddr = prev_head(curAddr, 0);
            }
        }
    }
    return resAddrs;
}

//--------------------------------------------------------------------------
// Find and mark gSmst global variable via EFI_SMM_BASE2_PROTOCOL_GUID
std::vector<ea_t> findSmstSmmBase(std::vector<ea_t> gBsList,
                                  std::vector<segment_t *> dataSegments) {
    std::vector<ea_t> resAddrs;
    efiGuid efiSmmBase2ProtocolGuid = {
        0xf4ccbfb7, 0xf6e0, 0x47fd, {0x9d, 0xd4, 0x10, 0xa8, 0xf1, 0x50, 0xc1, 0x91}};
    ea_t efiSmmBase2ProtocolGuidAddr = 0;

    for (auto seg_info : dataSegments) {
        if (seg_info == nullptr) {
            return resAddrs;
        }
        DEBUG_MSG("[%s] gSmst finding from 0x%016X to 0x%016X (via "
                  "EFI_SMM_BASE2_PROTOCOL_GUID)\n",
                  plugin_name, seg_info->start_ea, seg_info->end_ea);
        ea_t ea = seg_info->start_ea;
        while (ea != BADADDR && ea <= seg_info->end_ea - 15) {
            if (get_wide_dword(ea) == efiSmmBase2ProtocolGuid.data1) {
                efiSmmBase2ProtocolGuidAddr = ea;
                break;
            }
            ea += 1;
        }

        if (!efiSmmBase2ProtocolGuidAddr) {
            continue;
        }

        std::vector<ea_t> efiSmmBase2ProtocolGuidXrefs =
            getXrefs(efiSmmBase2ProtocolGuidAddr);

        for (std::vector<ea_t>::iterator guidXref = efiSmmBase2ProtocolGuidXrefs.begin();
             guidXref != efiSmmBase2ProtocolGuidXrefs.end(); ++guidXref) {
            ea_t resAddr = 0;
            ea_t curAddr = next_head(*guidXref, BADADDR);
            insn_t insn;

            // Check 16 instructions below
            for (auto i = 0; i < 16; i++) {
                decode_insn(&insn, curAddr);
                if (insn.itype == NN_lea && insn.ops[0].type == o_reg &&
                    insn.ops[0].reg == REG_RDX && insn.ops[1].type == o_mem) {
                    DEBUG_MSG("[%s] found gSmst at 0x%016X, address = 0x%016X\n",
                              plugin_name, curAddr, insn.ops[1].addr);
                    resAddr = insn.ops[1].addr;
                    if (find(gBsList.begin(), gBsList.end(), resAddr) != gBsList.end()) {
                        continue;
                    }
                    set_cmt(curAddr, "_EFI_SMM_SYSTEM_TABLE2 *gSmst;", true);
                    std::string hexstr = getHex(static_cast<uint64_t>(resAddr));
                    std::string name = "gSmst_" + hexstr;
                    setPtrTypeAndName(resAddr, name, "_EFI_SMM_SYSTEM_TABLE2");
                    resAddrs.push_back(resAddr);
                    break;
                }
                curAddr = next_head(curAddr, BADADDR);
            }
        }
    }
    return resAddrs;
}

//--------------------------------------------------------------------------
// Find SmiHandler in RegSwSmi function
std::vector<func_t *> findSmiHandlers(ea_t address) {
    std::vector<func_t *> smiHandlers;

    // Get RegSwSmi function
    func_t *regSmi = get_func(address);
    ea_t start = 0;
    ea_t ea = 0;
    insn_t insn;

    if (regSmi == nullptr) {
        DEBUG_MSG("[%s] can't get RegSwSmi function, will try to create it\n",
                  plugin_name)

        // Try to create function
        ea = address;
        for (int i = 0; i < 100; i++) {
            ea = prev_head(ea, 0);
            decode_insn(&insn, ea);
            if (insn.itype == NN_retn) {
                start = next_head(ea, BADADDR);
                break;
            }
        }

        // Create function
        add_func(start);
        regSmi = get_func(address);
        if (regSmi == nullptr) {
            return smiHandlers;
        }
    }

    // Find (SwDispath->Register)(SwDispath, SwSmiHandler, &SwSmiNum, Data)
    for (ea_t ea = regSmi->start_ea; ea <= regSmi->end_ea; ea = next_head(ea, BADADDR)) {
        decode_insn(&insn, ea);
        if (insn.itype == NN_callni) {

            // Find `lea r9`
            bool success = false;
            ea_t addr = prev_head(ea, 0);
            for (int i = 0; i < 12; i++) {
                decode_insn(&insn, addr);
                if (insn.itype == NN_lea && insn.ops[0].reg == REG_R9 &&
                    insn.ops[1].type == o_displ) {
                    success = true;
                    break;
                }
                addr = prev_head(addr, 0);
            }

            if (!success)
                continue;

            // Find `lea r8`
            success = false;
            addr = prev_head(ea, 0);
            for (int i = 0; i < 12; i++) {
                decode_insn(&insn, addr);
                if (insn.itype == NN_lea && insn.ops[0].reg == REG_R8 &&
                    insn.ops[1].type == o_displ) {
                    success = true;
                    break;
                }
                addr = prev_head(addr, 0);
            }

            if (!success)
                continue;

            // Find `lea rdx`
            success = false;
            addr = prev_head(ea, 0);
            for (int i = 0; i < 12; i++) {
                decode_insn(&insn, addr);
                if (insn.itype == NN_lea && insn.ops[0].reg == REG_RDX &&
                    insn.ops[1].type == o_mem) {
                    success = true;
                    break;
                }
                addr = prev_head(addr, 0);
            }

            if (!success)
                continue;

            ea_t smiHandlerAddr = insn.ops[1].addr;
            func_t *smiHandler = get_func(smiHandlerAddr);
            if (smiHandler == nullptr) {
                DEBUG_MSG("[%s] can't get SwSmiHandler function, will try to create it\n",
                          plugin_name);

                // Create function
                add_func(smiHandlerAddr);
                smiHandler = get_func(smiHandlerAddr);
            }

            if (smiHandler == nullptr) {
                continue;
            }

            // Make name for SwSmiHandler function
            std::string hexstr = getHex(static_cast<uint64_t>(smiHandler->start_ea));
            std::string name = "SwSmiHandler_" + hexstr;
            set_name(smiHandler->start_ea, name.c_str(), SN_CHECK);

            smiHandlers.push_back(smiHandler);
            DEBUG_MSG("[%s] found SmiHandler: 0x%016X\n", plugin_name,
                      smiHandler->start_ea);
        }
    }
    return smiHandlers;
}

//--------------------------------------------------------------------------
// Find SwSmiHandler function inside SMM drivers
//  * find EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID
//  * get EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID xref address
//  * this address will be inside RegSwSmi function
//  * find SmiHandler by pattern (instructions may be out of order)
//        lea     r9, ...
//        lea     r8, ...
//        lea     rdx, <func>
//        call    qword ptr [...]
std::vector<func_t *> findSmiHandlersSmmSwDispatch(std::vector<segment_t *> dataSegments,
                                                   std::vector<json> stackGuids) {
    DEBUG_MSG("[%s] SwSmiHandler function finding (using "
              "EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID)\n",
              plugin_name);
    efiGuid efiSmmSwDispatch2ProtocolGuid = {
        0x18a3c6dc, 0x5eea, 0x48c8, {0xa1, 0xc1, 0xb5, 0x33, 0x89, 0xf9, 0x89, 0x99}};
    efiGuid efiSmmSwDispatchProtocolGuid = {
        0xe541b773, 0xdd11, 0x420c, {0xb0, 0x26, 0xdf, 0x99, 0x36, 0x53, 0xf8, 0xbf}};
    std::vector<func_t *> smiHandlers;

    for (auto seg_info : dataSegments) {
        DEBUG_MSG("[%s] SwSmiHandler function finding from 0x%016X to 0x%016X\n",
                  plugin_name, seg_info->start_ea, seg_info->end_ea);
        ea_t efiSmmSwDispatchProtocolGuidAddr = 0;
        ea_t ea = seg_info->start_ea;
        while (ea != BADADDR && ea <= seg_info->end_ea - 15) {
            if (get_wide_dword(ea) == efiSmmSwDispatchProtocolGuid.data1 ||
                get_wide_dword(ea) == efiSmmSwDispatch2ProtocolGuid.data1) {
                efiSmmSwDispatchProtocolGuidAddr = ea;
                break;
            }
            ea += 1;
        }

        if (!efiSmmSwDispatchProtocolGuidAddr) {
            DEBUG_MSG("[%s] can't find a EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID guid\n",
                      plugin_name);
            continue;
        }

        DEBUG_MSG("[%s] EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID address: 0x%016X\n",
                  plugin_name, efiSmmSwDispatchProtocolGuidAddr);
        std::vector<ea_t> efiSmmSwDispatchProtocolGuidXrefs =
            getXrefs(efiSmmSwDispatchProtocolGuidAddr);

        for (std::vector<ea_t>::iterator guidXref =
                 efiSmmSwDispatchProtocolGuidXrefs.begin();
             guidXref != efiSmmSwDispatchProtocolGuidXrefs.end(); ++guidXref) {
            DEBUG_MSG("[%s] EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID xref address: "
                      "0x%016X\n",
                      plugin_name, *guidXref);
            std::vector<func_t *> smiHandlersCur = findSmiHandlers(*guidXref);
            smiHandlers.insert(smiHandlers.end(), smiHandlersCur.begin(),
                               smiHandlersCur.end());
        }
    }

    // Append stackSmiHandlers to result
    std::vector<func_t *> stackSmiHandlers =
        findSmiHandlersSmmSwDispatchStack(stackGuids);
    smiHandlers.insert(smiHandlers.end(), stackSmiHandlers.begin(),
                       stackSmiHandlers.end());
    return smiHandlers;
}

//--------------------------------------------------------------------------
// Find SwSmiHandler function inside SMM drivers in case where
// EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID is a local variable
std::vector<func_t *> findSmiHandlersSmmSwDispatchStack(std::vector<json> stackGuids) {
    std::vector<func_t *> smiHandlers;

    for (auto guid : stackGuids) {
        std::string name = static_cast<std::string>(guid["name"]);

        if (name != "EFI_SMM_SW_DISPATCH2_PROTOCOL_GUID" &&
            name != "EFI_SMM_SW_DISPATCH_PROTOCOL_GUID") {
            continue;
        }

        ea_t address = static_cast<ea_t>(guid["address"]);
        DEBUG_MSG("[%s] found EFI_SMM_SW_DISPATCH(2)_PROTOCOL_GUID on stack: "
                  "0x%016X\n",
                  plugin_name, address);
        std::vector<func_t *> smiHandlersCur = findSmiHandlers(address);
        smiHandlers.insert(smiHandlers.end(), smiHandlersCur.begin(),
                           smiHandlersCur.end());
    }

    return smiHandlers;
}

//--------------------------------------------------------------------------
// Find gSmmVar->SmmGetVariable calls via EFI_SMM_VARIABLE_PROTOCOL_GUID
std::vector<ea_t> findSmmGetVariableCalls(std::vector<segment_t *> dataSegments,
                                          std::vector<json> *allServices) {
    DEBUG_MSG("[%s] gSmmVar->SmmGetVariable calls finding via "
              "EFI_SMM_VARIABLE_PROTOCOL_GUID\n",
              plugin_name);
    efiGuid efiSmmVariableProtocolGuid = {
        0xed32d533, 0xc5a6, 0x40a2, {0xbd, 0xe2, 0x52, 0x55, 0x8d, 0x33, 0xcc, 0xa1}};
    std::vector<ea_t> smmGetVariableCalls;

    // Find all EFI_GUID EFI_SMM_VARIABLE_PROTOCOL_GUID addresses
    std::vector<ea_t> efiSmmVariableProtocolGuidAddrs;

    for (auto seg_info : dataSegments) {
        DEBUG_MSG("[%s] gSmmVar->SmmGetVariable function finding from 0x%016X "
                  "to 0x%016X\n",
                  plugin_name, seg_info->start_ea, seg_info->end_ea);
        ea_t ea = seg_info->start_ea;

        while (ea != BADADDR && ea <= seg_info->end_ea - 15) {
            if (get_wide_dword(ea) == efiSmmVariableProtocolGuid.data1) {
                efiSmmVariableProtocolGuidAddrs.push_back(ea);
                DEBUG_MSG("[%s] EFI_SMM_VARIABLE_PROTOCOL_GUID address: 0x%016X\n",
                          plugin_name, ea);
            }
            ea += 1;
        }

        if (!efiSmmVariableProtocolGuidAddrs.size()) {
            DEBUG_MSG("[%s] can't find a EFI_SMM_VARIABLE_PROTOCOL_GUID guid\n",
                      plugin_name);
            return smmGetVariableCalls;
        }
    }

    // Find all gSmmVar variables
    std::vector<ea_t> gSmmVarAddrs;
    for (std::vector<ea_t>::iterator guid_addr = efiSmmVariableProtocolGuidAddrs.begin();
         guid_addr != efiSmmVariableProtocolGuidAddrs.end(); ++guid_addr) {
        std::vector<ea_t> efiSmmVariableProtocolGuidXrefs = getXrefs(*guid_addr);

        for (auto guidXref : efiSmmVariableProtocolGuidXrefs) {
            segment_t *seg = getseg(static_cast<ea_t>(guidXref));
            qstring seg_name;
            get_segm_name(&seg_name, seg);
            DEBUG_MSG("[%s] EFI_SMM_VARIABLE_PROTOCOL_GUID xref address: 0x%016llX, "
                      "segment: %s\n",
                      plugin_name, guidXref, seg_name.c_str());

            size_t index = seg_name.find(".text");
            if (index == std::string::npos) {
                continue;
            }

            insn_t insn;
            ea_t ea = prev_head(static_cast<ea_t>(guidXref), 0);

            for (auto i = 0; i < 8; i++) {
                // Find `lea r8, <gSmmVar_addr>` instruction
                decode_insn(&insn, ea);
                if (insn.itype == NN_lea && insn.ops[0].type == o_reg &&
                    insn.ops[0].reg == REG_R8 && insn.ops[1].type == o_mem) {
                    DEBUG_MSG("[%s] gSmmVar address: 0x%016X\n", plugin_name,
                              insn.ops[1].addr);

                    // Set name and type
                    set_cmt(ea, "EFI_SMM_VARIABLE_PROTOCOL *gSmmVar", true);
                    std::string hexstr = getHex(static_cast<uint64_t>(insn.ops[1].addr));
                    std::string name = "gSmmVar_" + hexstr;
                    setPtrTypeAndName(insn.ops[1].addr, name,
                                      "EFI_SMM_VARIABLE_PROTOCOL");
                    gSmmVarAddrs.push_back(insn.ops[1].addr);
                    break;
                }
                ea = prev_head(ea, 0);
            }
        }
    }

    if (!gSmmVarAddrs.size()) {
        DEBUG_MSG("[%s] can't find gSmmVar addresses\n", plugin_name);
        return smmGetVariableCalls;
    }

    for (auto smmVarAddr : gSmmVarAddrs) {
        std::vector<ea_t> smmVarXrefs = getXrefs(static_cast<ea_t>(smmVarAddr));
        for (auto smmVarXref : smmVarXrefs) {
            segment_t *seg = getseg(static_cast<ea_t>(smmVarXref));
            qstring seg_name;
            get_segm_name(&seg_name, seg);
            DEBUG_MSG("[%s] gSmmVar xref address: 0x%016llX, segment: %s\n", plugin_name,
                      smmVarXref, seg_name.c_str());

            size_t index = seg_name.find(".text");
            if (index == std::string::npos) {
                continue;
            }

            uint16 gSmmVarReg = 0xffff;
            insn_t insn;
            ea_t ea = static_cast<ea_t>(smmVarXref);
            decode_insn(&insn, ea);

            if (insn.itype == NN_mov && insn.ops[0].type == o_reg &&
                insn.ops[1].type == o_mem) {
                gSmmVarReg = insn.ops[0].reg;
                for (auto i = 0; i < 16; i++) {
                    ea = next_head(ea, BADADDR);
                    decode_insn(&insn, ea);

                    if (insn.itype == NN_callni && gSmmVarReg == insn.ops[0].reg &&
                        insn.ops[0].addr == 0) {
                        DEBUG_MSG("[%s] gSmmVar->SmmGetVariable found: 0x%016X\n",
                                  plugin_name, ea);

                        if (find(smmGetVariableCalls.begin(), smmGetVariableCalls.end(),
                                 ea) == smmGetVariableCalls.end()) {
                            smmGetVariableCalls.push_back(ea);
                        }

                        // Temporarily add a "virtual" smm service call
                        // for easier annotations and UI

                        std::string cmt = getSmmVarComment();
                        set_cmt(ea, cmt.c_str(), true);
                        opStroff(ea, "EFI_SMM_VARIABLE_PROTOCOL");
                        DEBUG_MSG("[%s] 0x%016X : %s\n", plugin_name, ea,
                                  "SmmGetVariable");
                        std::string smm_call = "gSmmVar->SmmGetVariable";
                        json smmItem;
                        smmItem["address"] = ea;
                        smmItem["service_name"] = smm_call;
                        smmItem["table_name"] =
                            static_cast<std::string>("EFI_SMM_VARIABLE_PROTOCOL");
                        smmItem["offset"] = 0;

                        if (find(allServices->begin(), allServices->end(), smmItem) ==
                            allServices->end()) {
                            allServices->push_back(smmItem);
                        }

                        break;
                    }
                }
            }
        }
    }
    return smmGetVariableCalls;
}

std::vector<ea_t> resolveEfiSmmCpuProtocol(std::vector<json> stackGuids,
                                           std::vector<json> dataGuids,
                                           std::vector<json> *allServices) {
    std::vector<ea_t> readSaveStateCalls;
    DEBUG_MSG("[%s] Looking for EFI_SMM_CPU_PROTOCOL\n", plugin_name);
    std::vector<ea_t> codeAddrs;
    std::vector<ea_t> gSmmCpuAddrs;
    for (auto guid : stackGuids) {
        std::string name = static_cast<std::string>(guid["name"]);
        if (name != "EFI_SMM_CPU_PROTOCOL_GUID")
            continue;
        ea_t address = static_cast<ea_t>(guid["address"]);
        DEBUG_MSG("[%s] found EFI_SMM_CPU_PROTOCOL on stack: "
                  "0x%016X\n",
                  plugin_name, address);
        codeAddrs.push_back(address);
    }

    for (auto guid : dataGuids) {

        std::string name = static_cast<std::string>(guid["name"]);
        if (name != "EFI_SMM_CPU_PROTOCOL_GUID")
            continue;

        ea_t address = static_cast<ea_t>(guid["address"]);
        DEBUG_MSG("[%s] found EFI_SMM_CPU_PROTOCOL: 0x%016X\n", plugin_name, address);
        std::vector<ea_t> guidXrefs = getXrefs(address);

        for (auto guidXref : guidXrefs) {
            segment_t *seg = getseg(static_cast<ea_t>(guidXref));
            qstring seg_name;
            get_segm_name(&seg_name, seg);
            size_t index = seg_name.find(".text");
            if (index == std::string::npos) {
                continue;
            }
            codeAddrs.push_back(static_cast<ea_t>(guidXref));
        }
    }

    for (std::vector<ea_t>::iterator addr = codeAddrs.begin(); addr != codeAddrs.end();
         ++addr) {
        DEBUG_MSG("[%s] current address: 0x%016X\n", plugin_name, *addr)
        insn_t insn;
        ea_t ea = prev_head(*addr, 0);

        for (auto i = 0; i < 8; i++) {
            // Find 'lea r8, <gSmmCpu_addr>' instruction
            decode_insn(&insn, ea);
            if (insn.itype == NN_lea && insn.ops[0].type == o_reg &&
                insn.ops[0].reg == REG_R8 && insn.ops[1].type == o_mem) {
                DEBUG_MSG("[%s] gSmmCpu address: 0x%016X\n", plugin_name,
                          insn.ops[1].addr);

                // Set name and type
                set_cmt(ea, "EFI_SMM_CPU_PROTOCOL *gSmmCpu", true);
                std::string hexstr = getHex(static_cast<uint64_t>(insn.ops[1].addr));
                std::string name = "gSmmCpu_" + hexstr;
                setPtrTypeAndName(insn.ops[1].addr, name, "EFI_SMM_CPU_PROTOCOL");
                gSmmCpuAddrs.push_back(insn.ops[1].addr);
                break;
            }
            ea = prev_head(ea, 0);
        }
    }

    if (!gSmmCpuAddrs.size()) {
        DEBUG_MSG("[%s] can't find gSmmCpu addresses\n", plugin_name);
        return readSaveStateCalls;
    }

    for (auto smmCpu : gSmmCpuAddrs) {
        std::vector<ea_t> smmCpuXrefs = getXrefs(static_cast<ea_t>(smmCpu));

        for (auto smmCpuXref : smmCpuXrefs) {
            segment_t *seg = getseg(static_cast<ea_t>(smmCpuXref));
            qstring seg_name;
            get_segm_name(&seg_name, seg);
            size_t index = seg_name.find(".text");

            if (index == std::string::npos) {
                continue;
            }

            uint16_t gSmmCpuReg = 0xffff;
            insn_t insn;
            ea_t ea = static_cast<ea_t>(smmCpuXref);
            decode_insn(&insn, ea);

            if (insn.itype == NN_mov && insn.ops[0].type == o_reg &&
                insn.ops[1].type == o_mem) {
                gSmmCpuReg = insn.ops[0].reg;

                for (auto i = 0; i < 16; i++) {
                    ea = next_head(ea, BADADDR);
                    decode_insn(&insn, ea);

                    if (insn.itype == NN_callni && gSmmCpuReg == insn.ops[0].reg &&
                        insn.ops[0].addr == 0) {

                        if (find(readSaveStateCalls.begin(), readSaveStateCalls.end(),
                                 ea) == readSaveStateCalls.end()) {
                            readSaveStateCalls.push_back(ea);
                        }

                        opStroff(ea, "EFI_SMM_CPU_PROTOCOL");
                        DEBUG_MSG("[%s] 0x%016X : %s\n", plugin_name, ea,
                                  "gSmmCpu->ReadSaveState");
                        std::string smm_call = "gSmmCpu->ReadSaveState";
                        json smmItem;
                        smmItem["address"] = ea;
                        smmItem["service_name"] = smm_call;
                        smmItem["table_name"] =
                            static_cast<std::string>("EFI_SMM_CPU_PROTOCOL");
                        smmItem["offset"] = 0;

                        if (find(allServices->begin(), allServices->end(), smmItem) ==
                            allServices->end()) {
                            allServices->push_back(smmItem);
                        }

                        break;
                    }
                }
            }
        }
    }
    return readSaveStateCalls;
}