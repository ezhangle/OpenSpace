/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2024                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <openspace/documentation/documentationengine.h>

#include <openspace/openspace.h>
#include <openspace/documentation/core_registration.h>
#include <openspace/documentation/verifier.h>
#include <openspace/engine/openspaceengine.h>
#include <openspace/engine/globals.h>
#include <openspace/engine/configuration.h>
#include <openspace/events/eventengine.h>
#include <openspace/interaction/action.h>
#include <openspace/interaction/actionmanager.h>
#include <openspace/interaction/keybindingmanager.h>
#include <openspace/rendering/renderengine.h>
#include <openspace/json.h>
#include <openspace/scene/asset.h>
#include <openspace/scene/assetmanager.h>
#include <openspace/scene/scene.h>
#include <openspace/scripting/scriptscheduler.h>
#include <openspace/scripting/scriptengine.h>
#include <openspace/util/factorymanager.h>
#include <openspace/util/json_helper.h>
#include <ghoul/fmt.h>
#include <ghoul/filesystem/filesystem.h>
#include <ghoul/misc/profiling.h>

#include <fstream>
#include <future>

namespace openspace::documentation {

// Titles shown in the documentation sidebar 
constexpr const char* ScriptingName = "Scripting API";
constexpr const char* ActionName = "Actions";
constexpr const char* SceneName = "Scene";
constexpr const char* SettingsName = "Settings";
constexpr const char* EventsName = "Events";
constexpr const char* FactoryName = "Asset Types";
constexpr const char* KeybindingsName = "Keybindings";

// General keys
constexpr const char* NameKey = "name";
constexpr const char* IdentifierKey = "identifier";
constexpr const char* DescriptionKey = "description";
constexpr const char* DataKey = "data";
constexpr const char* TypeKey = "type";
constexpr const char* DocumentationKey = "documentation";

// Actions keys
constexpr const char* GuiNameKey = "guiName";

// Factory keys
constexpr const char* MembersKey = "members";
constexpr const char* OptionalKey = "optional";
constexpr const char* ReferenceKey = "reference";
constexpr const char* FoundKey = "found";
constexpr const char* RestrictionsKey = "restrictions";

// Properties keys
constexpr const char* PropertiesKeys = "properties";
constexpr const char* PropertyOwnersKey = "propertyOwners";
constexpr const char* TagsKey = "tags";
constexpr const char* UriKey = "uri";

// Scripting
constexpr const char* DefaultValueKey = "defaultValue";
constexpr const char* ArgumentsKey = "arguments";
constexpr const char* ReturnTypeKey = "returnType";
constexpr const char* HelpKey = "help";
constexpr const char* FileKey = "file";
constexpr const char* LineKey = "line";
constexpr const char* LibraryKey = "library";
constexpr const char* FullNameKey = "fullName";
constexpr const char* FunctionsKey = "functions";
constexpr const char* SourceLocationKey = "sourceLocation";
constexpr const char* OpenSpaceScriptingKey = "openspace";

// Licenses
constexpr const char* ProfileName = "Profile";
constexpr const char* AssetsName = "Assets";
constexpr const char* LicensesName = "Licenses";
constexpr const char* LicenseTypeName = "license";
constexpr const char* NoLicenseName = "No License";
constexpr const char* NoData = "";

constexpr const char* ProfileNameKey = "profileName";
constexpr const char* VersionKey = "version";
constexpr const char* AuthorKey = "author";
constexpr const char* UrlKey = "url";
constexpr const char* LicenseKey = "license";
constexpr const char* NoLicenseKey = "noLicense";
constexpr const char* IdentifiersKey = "identifiers";
constexpr const char* PathKey = "path";
constexpr const char* AssetKey = "assets";
constexpr const char* LicensesKey = "licenses";

// Factory
constexpr const char* OtherName = "Other";

constexpr const char* OtherIdentifierName = "other";
constexpr const char* propertyOwnerName = "propertyOwner";
constexpr const char* categoryName = "category";

constexpr const char* ClassesKey = "classes";

// Actions
constexpr const char* CommandKey = "command";

// Keybindings
constexpr const char* KeybindingsKey = "keybindings";
constexpr const char* ActionKey = "action";

nlohmann::json generateJsonDocumentation(const Documentation & d) {
    nlohmann::json json;

    json[NameKey] = d.name;
    json[IdentifierKey] = d.id;
    json[DescriptionKey] = d.description;
    json[MembersKey] = nlohmann::json::array();

    for (const DocumentationEntry& p : d.entries) {
        nlohmann::json entry;
        entry[NameKey] = p.key;
        entry[OptionalKey] = p.optional.value;
        entry[TypeKey] = p.verifier->type();
        entry[DocumentationKey] = p.documentation;

        TableVerifier* tv = dynamic_cast<TableVerifier*>(p.verifier.get());
        ReferencingVerifier* rv = dynamic_cast<ReferencingVerifier*>(p.verifier.get());

        if (rv) {
            const std::vector<Documentation>& documentations = DocEng.documentations();
            auto it = std::find_if(
                documentations.begin(),
                documentations.end(),
                [rv](const Documentation& doc) { return doc.id == rv->identifier; }
            );

            if (it == documentations.end()) {
                entry[ReferenceKey][FoundKey] = false;
            }
            else {
                nlohmann::json reference;
                reference[FoundKey] = true;
                reference[NameKey] = it->name;
                reference[IdentifierKey] = rv->identifier;

                entry[ReferenceKey] = reference;
            }
        }
        else if (tv) {
            Documentation doc = { .entries = tv->documentations };
            nlohmann::json restrictions = generateJsonDocumentation(doc);
            // We have a TableVerifier, so we need to recurse
            entry[RestrictionsKey] = restrictions;
        }
        else {
            entry[DescriptionKey] = p.verifier->documentation();
        }
        json[MembersKey].push_back(entry);
    }
    sortJson(json[MembersKey], NameKey);

    return json;
}

nlohmann::json createPropertyJson(openspace::properties::PropertyOwner* owner) {
    ZoneScoped;

    using namespace openspace;
    nlohmann::json json;
    json[NameKey] = !owner->guiName().empty() ? owner->guiName() : owner->identifier();

    json[DescriptionKey] = owner->description();
    json[PropertiesKeys] = nlohmann::json::array();
    json[PropertyOwnersKey] = nlohmann::json::array();
    json[TypeKey] = owner->type();
    json[TagsKey] = owner->tags();

    const std::vector<properties::Property*>& properties = owner->properties();
    for (properties::Property* p : properties) {
        nlohmann::json propertyJson;
        std::string name = !p->guiName().empty() ? p->guiName() : p->identifier();
        propertyJson[NameKey] = name;
        propertyJson[TypeKey] = p->className();
        propertyJson[UriKey] = p->fullyQualifiedIdentifier();
        propertyJson[IdentifierKey] = p->identifier();
        propertyJson[DescriptionKey] = p->description();

        json[PropertiesKeys].push_back(propertyJson);
    }
    sortJson(json[PropertiesKeys], NameKey);

    auto propertyOwners = owner->propertySubOwners();
    for (properties::PropertyOwner* o : propertyOwners) {
        nlohmann::json propertyOwner;
        json[PropertyOwnersKey].push_back(createPropertyJson(o));
    }
    sortJson(json[PropertyOwnersKey], NameKey);

    return json;
}

nlohmann::json LuaFunctionToJson(const openspace::scripting::LuaLibrary::Function& f,
    bool includeSourceLocation)
{
    using namespace openspace;
    using namespace openspace::scripting;
    nlohmann::json function;
    function[NameKey] = f.name;
    nlohmann::json arguments = nlohmann::json::array();

    for (const LuaLibrary::Function::Argument& arg : f.arguments) {
        nlohmann::json argument;
        argument[NameKey] = arg.name;
        argument[TypeKey] = arg.type;
        argument[DefaultValueKey] = arg.defaultValue.value_or(NoData);
        arguments.push_back(argument);
    }

    function[ArgumentsKey] = arguments;
    function[ReturnTypeKey] = f.returnType;
    function[HelpKey ] = f.helpText;

    if (includeSourceLocation) {
        nlohmann::json sourceLocation;
        sourceLocation[FileKey] = f.sourceLocation.file;
        sourceLocation[LineKey] = f.sourceLocation.line;
        function[SourceLocationKey] = sourceLocation;
    }

    return function;
}

DocumentationEngine* DocumentationEngine::_instance = nullptr;

DocumentationEngine::DuplicateDocumentationException::DuplicateDocumentationException(
                                                                        Documentation doc)
    : ghoul::RuntimeError(fmt::format(
        "Duplicate Documentation with name '{}' and id '{}'", doc.name, doc.id
    ))
    , documentation(std::move(doc))
{}

DocumentationEngine::DocumentationEngine() {}

void DocumentationEngine::initialize() {
    ghoul_assert(!isInitialized(), "DocumentationEngine is already initialized");
    _instance = new DocumentationEngine;
}

void DocumentationEngine::deinitialize() {
    ghoul_assert(isInitialized(), "DocumentationEngine is not initialized");
    delete _instance;
    _instance = nullptr;
}

bool DocumentationEngine::isInitialized() {
    return _instance != nullptr;
}

DocumentationEngine& DocumentationEngine::ref() {
    if (_instance == nullptr) {
        _instance = new DocumentationEngine;
        registerCoreClasses(*_instance);
    }
    return *_instance;
}

nlohmann::json DocumentationEngine::generateScriptEngineJson() const {
    ZoneScoped;

    using namespace openspace;
    using namespace scripting;
    const std::vector<LuaLibrary> libraries = global::scriptEngine->allLuaLibraries();
    nlohmann::json json;

    for (const LuaLibrary& l : libraries) {

        nlohmann::json library;
        std::string libraryName = l.name;
        // Keep the library key for backwards compatability
        library[LibraryKey] = libraryName;
        library[NameKey] = libraryName;
        std::string os = OpenSpaceScriptingKey;
        library[FullNameKey] = libraryName.empty() ? os : os + "." + libraryName;

        for (const LuaLibrary::Function& f : l.functions) {
            bool hasSourceLocation = true;
            library[FunctionsKey].push_back(LuaFunctionToJson(f, hasSourceLocation));
        }

        for (const LuaLibrary::Function& f : l.documentations) {
            bool hasSourceLocation = false;
            library[FunctionsKey].push_back(LuaFunctionToJson(f, hasSourceLocation));
        }
        sortJson(library[FunctionsKey], NameKey);
        json.push_back(library);

        sortJson(json, LibraryKey);
    }
    return json;
}

nlohmann::json DocumentationEngine::generateLicensesGroupedByLicense() const {
    nlohmann::json json;

    std::vector<const Asset*> assets =
        global::openSpaceEngine->assetManager().allAssets();

    int metaTotal = 0;
    for (const Asset* asset : assets) {
        std::optional<Asset::MetaInformation> meta = asset->metaInformation();
        if (!meta.has_value()) {
            continue;
        }
        metaTotal++;
    }

    if (global::profile->meta.has_value()) {
        metaTotal++;
        nlohmann::json metaJson;
        metaJson[NameKey] = ProfileName;
        metaJson[ProfileNameKey] = global::profile->meta->name.value_or(NoData);
        metaJson[VersionKey] = global::profile->meta->version.value_or(NoData);
        metaJson[DescriptionKey] = global::profile->meta->description.value_or(NoData);
        metaJson[AuthorKey] = global::profile->meta->author.value_or(NoData);
        metaJson[UrlKey] = global::profile->meta->url.value_or(NoData);
        metaJson[LicenseKey] = global::profile->meta->license.value_or(NoData);
        metaJson[TypeKey] = LicenseTypeName;
        json.push_back(std::move(metaJson));
    }

    std::map<std::string, nlohmann::json> assetLicenses;
    for (const Asset* asset : assets) {
        std::optional<Asset::MetaInformation> meta = asset->metaInformation();

        nlohmann::json assetJson;
        if (!meta.has_value()) {
            assetJson[NameKey] = NoData;
            assetJson[VersionKey] = NoData;
            assetJson[DescriptionKey] = NoData;
            assetJson[AuthorKey] = NoData;
            assetJson[UrlKey] = NoData;
            assetJson[LicenseKey] = NoLicenseName;
            assetJson[IdentifiersKey] = NoData;
            assetJson[PathKey] = asset->path().string();

            assetLicenses[NoLicenseKey].push_back(assetJson);
        }
        else {
            std::string licenseName = meta->license == NoData ? NoLicenseName :
                meta->license;
            assetJson[NameKey] = meta->name;
            assetJson[VersionKey] = meta->version;
            assetJson[DescriptionKey] = meta->description;
            assetJson[AuthorKey] = meta->author;
            assetJson[UrlKey] = meta->url;
            assetJson[LicenseKey] = licenseName;
            assetJson[IdentifiersKey] = meta->identifiers;
            assetJson[PathKey] = asset->path().string();

            assetLicenses[licenseName].push_back(assetJson);
        }
    }

    nlohmann::json assetsJson;
    assetsJson[NameKey] = AssetsName;
    assetsJson[TypeKey] = LicensesName;

    using K = const std::string;
    using V = nlohmann::json;
    for (const std::pair<K, V>& assetLicense : assetLicenses) {
        nlohmann::json entry;
        entry[NameKey] = assetLicense.first;
        entry[AssetKey] = assetLicense.second;
        sortJson(entry[AssetKey], NameKey);
        assetsJson[LicensesKey].push_back(entry);
    }
    json.push_back(assetsJson);

    nlohmann::json result;
    result[NameKey] = LicensesName;
    result[DataKey] = json;

    return result;
}

nlohmann::json DocumentationEngine::generateLicenseList() const {
    nlohmann::json json;

    if (global::profile->meta.has_value()) {
        nlohmann::json profile;
        profile[NameKey] = global::profile->meta->name.value_or(NoData);
        profile[VersionKey] = global::profile->meta->version.value_or(NoData);
        profile[DescriptionKey] = global::profile->meta->description.value_or(NoData);
        profile[AuthorKey] = global::profile->meta->author.value_or(NoData);
        profile[UrlKey] = global::profile->meta->url.value_or(NoData);
        profile[LicenseKey] = global::profile->meta->license.value_or(NoData);
        json.push_back(profile);
    }

    std::vector<const Asset*> assets =
        global::openSpaceEngine->assetManager().allAssets();

    for (const Asset* asset : assets) {
        std::optional<Asset::MetaInformation> meta = asset->metaInformation();

        if (!meta.has_value()) {
            continue;
        }

        nlohmann::json assetJson;
        assetJson[NameKey] = meta->name;
        assetJson[VersionKey] = meta->version;
        assetJson[DescriptionKey] = meta->description;
        assetJson[AuthorKey] = meta->author;
        assetJson[UrlKey] = meta->url;
        assetJson[LicenseKey] = meta->license;
        assetJson[IdentifiersKey] = meta->identifiers;
        assetJson[PathKey] = asset->path().string();

        json.push_back(assetJson);
    }
    return json;
}

nlohmann::json DocumentationEngine::generateEventJson() const {
    nlohmann::json result;
    result[NameKey] = EventsName;
    nlohmann::json data = nlohmann::json::array();

    std::vector<EventEngine::ActionInfo> eventActions =
        global::eventEngine->registeredActions();

    for (const EventEngine::ActionInfo& eventAction : eventActions) {
        nlohmann::json eventJson;
        std::string eventName = std::string(events::toString(eventAction.type));
        eventJson[NameKey] = eventName;
        data.push_back(eventJson);
    }

    result[DataKey] = data;
    return result;
}

nlohmann::json DocumentationEngine::generateFactoryManagerJson() const {                 
    nlohmann::json json;

    std::vector<Documentation> docs = _documentations; // Copy the documentations
    const std::vector<FactoryManager::FactoryInfo>& factories =
        FactoryManager::ref().factories();

    for (const FactoryManager::FactoryInfo& factoryInfo : factories) {
        nlohmann::json factory;
        factory[NameKey] = factoryInfo.name;
        factory[IdentifierKey] = categoryName + factoryInfo.name;

        ghoul::TemplateFactoryBase* f = factoryInfo.factory.get();
        // Add documentation about base class
        auto factoryDoc = std::find_if(
            docs.begin(),
            docs.end(),
            [&factoryInfo](const Documentation& d) {
                return d.name == factoryInfo.name;
            });
        if (factoryDoc != docs.end()) {
            nlohmann::json documentation = generateJsonDocumentation(*factoryDoc);
            factory[ClassesKey].push_back(documentation);
            // Remove documentation from list check at the end if all docs got put in
            docs.erase(factoryDoc);
        }
        else {
            nlohmann::json documentation;
            documentation[NameKey] = factoryInfo.name;
            documentation[IdentifierKey] = factoryInfo.name;
            factory[ClassesKey].push_back(documentation);
        }

        // Add documentation about derived classes
        const std::vector<std::string>& registeredClasses = f->registeredClasses();
        for (const std::string& c : registeredClasses) {
            auto found = std::find_if(
                docs.begin(),
                docs.end(),
                [&c](const Documentation& d) {
                    return d.name == c;
                });
            if (found != docs.end()) {
                nlohmann::json documentation = generateJsonDocumentation(*found);
                factory[ClassesKey].push_back(documentation);
                docs.erase(found);
            }
            else {
                nlohmann::json documentation;
                documentation[NameKey] = c;
                documentation[IdentifierKey] = c;
                factory[ClassesKey].push_back(documentation);
            }
        }
        sortJson(factory[ClassesKey], NameKey);
        json.push_back(factory);
    }
    // Add all leftover docs
    nlohmann::json leftovers;
    leftovers[NameKey] = OtherName;
    leftovers[IdentifierKey] = OtherIdentifierName;

    for (const Documentation& doc : docs) {
        leftovers[ClassesKey].push_back(generateJsonDocumentation(doc));
    }
    sortJson(leftovers[ClassesKey], NameKey);
    json.push_back(leftovers);
    sortJson(json, NameKey);

    // I did not check the output of this for correctness ---abock
    nlohmann::json result;
    result[NameKey] = FactoryName;
    result[DataKey] = json;

    return result;
}

nlohmann::json DocumentationEngine::generateKeybindingsJson() const {
    ZoneScoped;

    nlohmann::json json;
    const std::multimap<KeyWithModifier, std::string>& luaKeys =
        global::keybindingManager->keyBindings();

    for (const std::pair<const KeyWithModifier, std::string>& p : luaKeys) {
        nlohmann::json keybind;
        keybind[NameKey] = ghoul::to_string(p.first);
        keybind[ActionKey] = p.second;
        json.push_back(std::move(keybind));
    }
    sortJson(json, NameKey);

    nlohmann::json result;
    result[NameKey] = KeybindingsName;
    result[KeybindingsKey] = json;
    return result;
}

nlohmann::json DocumentationEngine::generatePropertyOwnerJson(
    properties::PropertyOwner* owner) const {
    ZoneScoped;

    nlohmann::json json;
    std::vector<properties::PropertyOwner*> subOwners = owner->propertySubOwners();
    for (properties::PropertyOwner* o : subOwners) {
        if (o->identifier() != SceneName) {
            nlohmann::json jsonOwner = createPropertyJson(o);

            json.push_back(jsonOwner);
        }
    }
    sortJson(json, NameKey);

    nlohmann::json result;
    result[NameKey] = propertyOwnerName;
    result[DataKey] = json;

    return result;
}

void DocumentationEngine::writeDocumentation() const {
    ZoneScoped;

    // Write documentation to json files if config file supplies path for doc files
    std::string path = global::configuration->documentation.path;
    if (path.empty()) {
        // if path was empty, that means that no documentation is requested
        return;
    }
    path = absPath(path).string() + '/';

    // Start the async requests as soon as possible so they are finished when we need them
    std::future<nlohmann::json> settings = std::async(
        &DocumentationEngine::generatePropertyOwnerJson,
        this,
        global::rootPropertyOwner
    );

    std::future<nlohmann::json> sceneJson = std::async(
        &DocumentationEngine::generatePropertyOwnerJson,
        this,
        global::renderEngine->scene()
    );

    nlohmann::json scripting = generateScriptEngineJson();
    nlohmann::json factory = generateFactoryManagerJson();
    nlohmann::json keybindings = generateKeybindingsJson();
    nlohmann::json license = generateLicensesGroupedByLicense();
    nlohmann::json sceneProperties = settings.get();
    nlohmann::json sceneGraph = sceneJson.get();
    nlohmann::json actions = generateActionJson();
    nlohmann::json events = generateEventJson();

    sceneProperties[NameKey] = SettingsName;
    sceneGraph[NameKey] = SceneName;

    // Add this here so that the generateJson function is the same as before to ensure
    // backwards compatibility
    nlohmann::json scriptingResult;
    scriptingResult[NameKey] = ScriptingName;
    scriptingResult[DataKey] = scripting;

    nlohmann::json documentation = {
        sceneGraph, sceneProperties, actions, events, keybindings, license,
        scriptingResult, factory
    };

    nlohmann::json result;
    result[DocumentationKey] = documentation;

    std::ofstream out(absPath("${DOCUMENTATION}/documentationData.js"));
    out << "var data = " << result.dump();
    out.close();
}

nlohmann::json DocumentationEngine::generateActionJson() const {
    using namespace interaction;

    nlohmann::json res;
    res[NameKey] = ActionName;
    res[DataKey] = nlohmann::json::array();
    std::vector<Action> actions = global::actionManager->actions();

    for (const Action& action : actions) {
        nlohmann::json d;
        // Use identifier as name to make it more similar to scripting api
        d[NameKey] = action.identifier;
        d[GuiNameKey] = action.name;
        d[DocumentationKey] = action.documentation;
        d[CommandKey] = action.command;
        res[DataKey].push_back(d);
    }
    sortJson(res[DataKey], NameKey);
    return res;
}


void DocumentationEngine::addDocumentation(Documentation documentation) {
    if (documentation.id.empty()) {
        _documentations.push_back(std::move(documentation));
    }
    else {
        auto it = std::find_if(
            _documentations.begin(),
            _documentations.end(),
            [documentation](const Documentation& d) { return documentation.id == d.id; }
        );

        if (it != _documentations.end()) {
            throw DuplicateDocumentationException(std::move(documentation));
        }
        else {
            _documentations.push_back(std::move(documentation));
        }
    }
}

std::vector<Documentation> DocumentationEngine::documentations() const {
    return _documentations;
}
} // namespace openspace::documentation
