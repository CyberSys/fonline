//      __________        ___               ______            _
//     / ____/ __ \____  / (_)___  ___     / ____/___  ____ _(_)___  ___
//    / /_  / / / / __ \/ / / __ \/ _ \   / __/ / __ \/ __ `/ / __ \/ _ `
//   / __/ / /_/ / / / / / / / / /  __/  / /___/ / / / /_/ / / / / /  __/
//  /_/    \____/_/ /_/_/_/_/ /_/\___/  /_____/_/ /_/\__, /_/_/ /_/\___/
//                                                  /____/
// FOnline Engine
// https://fonline.ru
// https://github.com/cvet/fonline
//
// MIT License
//
// Copyright (c) 2006 - 2023, Anton Tsvetinskiy aka cvet <cvet@tut.by>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "ProtoManager.h"
#include "ConfigFile.h"
#include "EngineBase.h"
#include "FileSystem.h"
#include "StringUtils.h"

template<class T>
static void WriteProtosToBinary(vector<uint8>& data, const unordered_map<hstring, const T*>& protos)
{
    STACK_TRACE_ENTRY();

    vector<uint8> protos_data;
    auto writer = DataWriter(protos_data);

    set<hstring> str_hashes;
    vector<uint8> props_data;

    for (auto& kv : protos) {
        auto* proto_item = kv.second;

        const auto proto_name = proto_item->GetName();
        writer.Write<uint16>(static_cast<uint16>(proto_name.length()));
        writer.WritePtr(proto_name.data(), proto_name.length());

        writer.Write<uint16>(static_cast<uint16>(proto_item->GetComponents().size()));
        for (const auto& component : proto_item->GetComponents()) {
            auto&& component_str = component.as_str();
            writer.Write<uint16>(static_cast<uint16>(component_str.length()));
            writer.WritePtr(component_str.data(), component_str.length());
        }

        proto_item->GetProperties().StoreAllData(props_data, str_hashes);
        writer.Write<uint>(static_cast<uint>(props_data.size()));
        writer.WritePtr(props_data.data(), props_data.size());
    }

    auto final_writer = DataWriter(data);
    final_writer.Write<uint>(static_cast<uint>(str_hashes.size()));
    for (const auto& hstr : str_hashes) {
        const auto& str = hstr.as_str();
        final_writer.Write<uint>(static_cast<uint>(str.length()));
        final_writer.WritePtr(str.c_str(), str.length());
    }
    final_writer.Write<uint>(static_cast<uint>(protos.size()));
    final_writer.WritePtr(protos_data.data(), protos_data.size());
}

template<class T>
static void ReadProtosFromBinary(HashResolver& hash_resolver, const PropertyRegistrator* property_registrator, DataReader& reader, unordered_map<hstring, const T*>& protos)
{
    STACK_TRACE_ENTRY();

    const auto hashes_count = reader.Read<uint>();

    string str;
    for (uint i = 0; i < hashes_count; i++) {
        const auto str_len = reader.Read<uint>();
        str.resize(str_len);
        reader.ReadPtr(str.data(), str.length());
        const auto hstr = hash_resolver.ToHashedString(str);
        UNUSED_VARIABLE(hstr);
    }

    vector<uint8> props_data;

    const auto protos_count = reader.Read<uint>();
    for (uint i = 0; i < protos_count; i++) {
        const auto proto_name_len = reader.Read<uint16>();
        const auto proto_name = string(reader.ReadPtr<char>(proto_name_len), proto_name_len);
        const auto proto_id = hash_resolver.ToHashedString(proto_name);

        auto* proto = new T(proto_id, property_registrator);

        const auto components_count = reader.Read<uint16>();
        for (uint16 j = 0; j < components_count; j++) {
            const auto component_name_len = reader.Read<uint16>();
            const auto component_name = string(reader.ReadPtr<char>(component_name_len), component_name_len);
            const auto component_name_hashed = hash_resolver.ToHashedString(component_name);
            RUNTIME_ASSERT(property_registrator->IsComponentRegistered(component_name_hashed));
            proto->EnableComponent(component_name_hashed);
        }

        const uint data_size = reader.Read<uint>();
        props_data.resize(data_size);
        reader.ReadPtr<uint8>(props_data.data(), data_size);
        proto->GetPropertiesForEdit().RestoreAllData(props_data);

        RUNTIME_ASSERT(!protos.count(proto_id));
        protos.emplace(proto_id, proto);
    }
}

static void InsertMapValues(const map<string, string>& from_kv, map<string, string>& to_kv, bool overwrite)
{
    STACK_TRACE_ENTRY();

    for (auto&& [key, value] : from_kv) {
        RUNTIME_ASSERT(!key.empty());

        if (key[0] != '$') {
            if (overwrite) {
                to_kv[key] = value;
            }
            else {
                to_kv.emplace(key, value);
            }
        }
        else if (key == "$Components" && !value.empty()) {
            if (to_kv.count("$Components") == 0) {
                to_kv["$Components"] = value;
            }
            else {
                to_kv["$Components"] += " " + value;
            }
        }
    }
}

template<class T>
static void ParseProtosExt(FileSystem& resources, HashResolver& hash_resolver, const PropertyRegistrator* property_registrator, string_view ext, string_view section_name, unordered_map<hstring, const T*>& protos)
{
    STACK_TRACE_ENTRY();

    // Collect data
    auto files = resources.FilterFiles(ext);
    map<hstring, map<string, string>> files_protos;
    map<hstring, map<string, map<string, string>>> files_texts;
    while (files.MoveNext()) {
        auto file = files.GetCurFile();

        auto fopro_options = ConfigFileOption::None;
        if constexpr (std::is_same_v<T, ProtoMap>) {
            fopro_options = ConfigFileOption::ReadFirstSection;
        }

        auto fopro = ConfigFile(file.GetPath(), file.GetStr(), &hash_resolver, fopro_options);

        auto protos_data = fopro.GetSections(section_name);
        if constexpr (std::is_same_v<T, ProtoMap>) {
            if (protos_data.empty()) {
                protos_data = fopro.GetSections("Header");
            }
        }

        for (auto& pkv : protos_data) {
            auto& kv = *pkv;
            auto name = kv.count("$Name") ? kv["$Name"] : file.GetName();
            auto pid = hash_resolver.ToHashedString(name);
            if (files_protos.count(pid) != 0) {
                throw ProtoManagerException("Proto already loaded", name);
            }

            files_protos.emplace(pid, kv);

            for (const auto& section : fopro.GetSectionNames()) {
                if (section.size() == "Text_xxxx"_len && _str(section).startsWith("Text_")) {
                    if (!files_texts.count(pid)) {
                        map<string, map<string, string>> texts;
                        files_texts.emplace(pid, texts);
                    }
                    files_texts[pid].emplace(section, fopro.GetSection(section));
                }
            }
        }

        if (protos_data.empty()) {
            throw ProtoManagerException("File does not contain any proto", file.GetName());
        }
    }

    // Injection
    auto injection = [&files_protos, &hash_resolver](const string& key_name, bool overwrite) {
        for (auto&& [pid, kv] : files_protos) {
            if (kv.count(key_name)) {
                for (const auto& inject_name : _str(kv[key_name]).split(' ')) {
                    if (inject_name == "All") {
                        for (auto&& [pid2, kv2] : files_protos) {
                            if (pid2 != pid) {
                                InsertMapValues(kv, kv2, overwrite);
                            }
                        }
                    }
                    else {
                        auto inject_name_hashed = hash_resolver.ToHashedString(inject_name);
                        if (!files_protos.count(inject_name_hashed)) {
                            throw ProtoManagerException("Proto not found for injection from another proto", inject_name, pid);
                        }
                        InsertMapValues(kv, files_protos[inject_name_hashed], overwrite);
                    }
                }
            }
        }
    };
    injection("$Inject", false);

    // Protos
    for (auto&& [pid, kv] : files_protos) {
        auto base_name = pid.as_str();
        RUNTIME_ASSERT(protos.count(pid) == 0);

        // Fill content from parents
        map<string, string> final_kv;
        std::function<void(string_view, map<string, string>&)> fill_parent = [&fill_parent, &base_name, &files_protos, &final_kv, &hash_resolver](string_view name, map<string, string>& cur_kv) {
            const auto parent_name_line = cur_kv.count("$Parent") ? cur_kv["$Parent"] : string();
            for (auto& parent_name : _str(parent_name_line).split(' ')) {
                const auto parent_pid = hash_resolver.ToHashedString(parent_name);
                auto parent = files_protos.find(parent_pid);
                if (parent == files_protos.end()) {
                    if (base_name == name) {
                        throw ProtoManagerException("Proto fail to load parent", base_name, parent_name);
                    }

                    throw ProtoManagerException("Proto fail to load parent for another proto", base_name, parent_name, name);
                }

                fill_parent(parent_name, parent->second);
                InsertMapValues(parent->second, final_kv, true);
            }
        };
        fill_parent(base_name, kv);

        // Actual content
        InsertMapValues(kv, final_kv, true);

        // Final injection
        injection("$InjectOverride", true);

        // Create proto
        auto props = Properties(property_registrator);
        if (!props.ApplyFromText(final_kv)) {
            throw ProtoManagerException("Proto item fail to load properties", base_name);
        }

        auto* proto = new T(pid, property_registrator, &props);

        // Components
        if (final_kv.count("$Components")) {
            for (const auto& component_name : _str(final_kv["$Components"]).split(' ')) {
                const auto component_name_hashed = hash_resolver.ToHashedString(component_name);
                if (!proto->GetProperties().GetRegistrator()->IsComponentRegistered(component_name_hashed)) {
                    throw ProtoManagerException("Proto item has invalid component", base_name, component_name);
                }
                proto->EnableComponent(component_name_hashed);
            }
        }

        // Add to collection
        protos.emplace(pid, proto);
    }

    // Texts
    for (auto&& [pid, file_text] : files_texts) {
        auto* proto = const_cast<T*>(protos[pid]);
        RUNTIME_ASSERT(proto);

        for (auto&& [lang, pairs] : file_text) {
            TextPack temp_text_pack;
            temp_text_pack.LoadFromMap(pairs);

            TextPack text_pack;
            uint str_num = 0;

            while ((str_num = temp_text_pack.GetStrNumUpper(str_num)) != 0) {
                const size_t count = temp_text_pack.GetStrCount(str_num);
                uint new_str_num = str_num;

                if constexpr (std::is_same_v<T, ProtoItem>) {
                    new_str_num = ITEM_STR_ID(proto->GetProtoId().as_uint(), str_num);
                }
                else if constexpr (std::is_same_v<T, ProtoCritter>) {
                    new_str_num = CR_STR_ID(proto->GetProtoId().as_uint(), str_num);
                }
                else if constexpr (std::is_same_v<T, ProtoLocation>) {
                    new_str_num = LOC_STR_ID(proto->GetProtoId().as_uint(), str_num);
                }

                for (const auto n : xrange(count)) {
                    text_pack.AddStr(new_str_num, temp_text_pack.GetStr(str_num, n));
                }
            }

            proto->Texts.emplace_back(lang.substr("Text_"_len), std::move(text_pack));
        }
    }
}

ProtoManager::ProtoManager(FOEngineBase* engine) :
    _engine {engine}
{
    STACK_TRACE_ENTRY();
}

void ProtoManager::ParseProtos(FileSystem& resources)
{
    STACK_TRACE_ENTRY();

    ParseProtosExt<ProtoItem>(resources, *_engine, _engine->GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), "foitem", "ProtoItem", _itemProtos);
    ParseProtosExt<ProtoCritter>(resources, *_engine, _engine->GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME), "focr", "ProtoCritter", _crProtos);
    ParseProtosExt<ProtoMap>(resources, *_engine, _engine->GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME), "fomap", "ProtoMap", _mapProtos);
    ParseProtosExt<ProtoLocation>(resources, *_engine, _engine->GetPropertyRegistrator(LocationProperties::ENTITY_CLASS_NAME), "foloc", "ProtoLocation", _locProtos);

    // Mapper collections
    for (auto&& [pid, proto] : _itemProtos) {
        if (!proto->GetComponents().empty()) {
            const_cast<ProtoItem*>(proto)->CollectionName = _str(*proto->GetComponents().begin()).lower();
        }
        else {
            const_cast<ProtoItem*>(proto)->CollectionName = "other";
        }
    }

    for (auto&& [pid, proto] : _crProtos) {
        const_cast<ProtoCritter*>(proto)->CollectionName = "all";
    }
}

void ProtoManager::LoadFromResources()
{
    STACK_TRACE_ENTRY();

    string protos_fname = "Protos.foprob";

#if !FO_SINGLEPLAYER
    switch (_engine->GetPropertiesRelation()) {
    case PropertiesRelationType::BothRelative:
        protos_fname = "FullProtos.foprob";
        break;
    case PropertiesRelationType::ServerRelative:
        protos_fname = "ServerProtos.foprob";
        break;
    case PropertiesRelationType::ClientRelative:
        protos_fname = "ClientProtos.foprob";
        break;
    }
#endif

    const auto protos_file = _engine->Resources.ReadFile(protos_fname);
    if (!protos_file) {
        throw ProtoManagerException("Protos binary file not found", protos_fname);
    }

    auto reader = DataReader({protos_file.GetBuf(), protos_file.GetSize()});
    ReadProtosFromBinary<ProtoItem>(*_engine, _engine->GetPropertyRegistrator(ItemProperties::ENTITY_CLASS_NAME), reader, _itemProtos);
    ReadProtosFromBinary<ProtoCritter>(*_engine, _engine->GetPropertyRegistrator(CritterProperties::ENTITY_CLASS_NAME), reader, _crProtos);
    ReadProtosFromBinary<ProtoMap>(*_engine, _engine->GetPropertyRegistrator(MapProperties::ENTITY_CLASS_NAME), reader, _mapProtos);
    ReadProtosFromBinary<ProtoLocation>(*_engine, _engine->GetPropertyRegistrator(LocationProperties::ENTITY_CLASS_NAME), reader, _locProtos);
    reader.VerifyEnd();
}

auto ProtoManager::GetProtosBinaryData() const -> vector<uint8>
{
    STACK_TRACE_ENTRY();

    vector<uint8> data;
    WriteProtosToBinary<ProtoItem>(data, _itemProtos);
    WriteProtosToBinary<ProtoCritter>(data, _crProtos);
    WriteProtosToBinary<ProtoMap>(data, _mapProtos);
    WriteProtosToBinary<ProtoLocation>(data, _locProtos);
    return data;
}

auto ProtoManager::GetProtoItem(hstring proto_id) -> const ProtoItem*
{
    STACK_TRACE_ENTRY();

    const auto it = _itemProtos.find(proto_id);
    return it != _itemProtos.end() ? it->second : nullptr;
}

auto ProtoManager::GetProtoCritter(hstring proto_id) -> const ProtoCritter*
{
    STACK_TRACE_ENTRY();

    const auto it = _crProtos.find(proto_id);
    return it != _crProtos.end() ? it->second : nullptr;
}

auto ProtoManager::GetProtoMap(hstring proto_id) -> const ProtoMap*
{
    STACK_TRACE_ENTRY();

    const auto it = _mapProtos.find(proto_id);
    return it != _mapProtos.end() ? it->second : nullptr;
}

auto ProtoManager::GetProtoLocation(hstring proto_id) -> const ProtoLocation*
{
    STACK_TRACE_ENTRY();

    const auto it = _locProtos.find(proto_id);
    return it != _locProtos.end() ? it->second : nullptr;
}

auto ProtoManager::GetProtoItems() const -> const unordered_map<hstring, const ProtoItem*>&
{
    STACK_TRACE_ENTRY();

    return _itemProtos;
}

auto ProtoManager::GetProtoCritters() const -> const unordered_map<hstring, const ProtoCritter*>&
{
    STACK_TRACE_ENTRY();

    return _crProtos;
}

auto ProtoManager::GetProtoMaps() const -> const unordered_map<hstring, const ProtoMap*>&
{
    STACK_TRACE_ENTRY();

    return _mapProtos;
}

auto ProtoManager::GetProtoLocations() const -> const unordered_map<hstring, const ProtoLocation*>&
{
    STACK_TRACE_ENTRY();

    return _locProtos;
}

auto ProtoManager::GetAllProtos() const -> vector<const ProtoEntity*>
{
    STACK_TRACE_ENTRY();

    vector<const ProtoEntity*> protos;
    protos.reserve(_itemProtos.size() + _crProtos.size() + _mapProtos.size() + _locProtos.size());

    for (auto&& [id, proto] : _itemProtos) {
        protos.push_back(proto);
    }
    for (auto&& [id, proto] : _crProtos) {
        protos.push_back(proto);
    }
    for (auto&& [id, proto] : _mapProtos) {
        protos.push_back(proto);
    }
    for (auto&& [id, proto] : _locProtos) {
        protos.push_back(proto);
    }

    return protos;
}
