#ifndef YICAD_PLUGIN_ABI_H
#define YICAD_PLUGIN_ABI_H

#include <stddef.h>
#include <stdint.h>

#if !defined(__cplusplus) && \
    (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 201112L)
#error "YiCadPluginAbi.h requires C11 or later"
#endif

/** @brief 冻结的 C ABI v1 版本号。 */
#define YICAD_PLUGIN_ABI_V1 UINT32_C(1)
/** @brief 当前 SDK 支持的最低 C ABI 版本。 */
#define YICAD_PLUGIN_ABI_MIN_VERSION YICAD_PLUGIN_ABI_V1
/** @brief 当前 SDK 支持的最高 C ABI 版本。 */
#define YICAD_PLUGIN_ABI_MAX_VERSION YICAD_PLUGIN_ABI_V1
/** @brief 当前 C ABI 版本；保留该名称以兼容 ABI v1 插件源码。 */
#define YICAD_PLUGIN_ABI_VERSION YICAD_PLUGIN_ABI_MAX_VERSION

#if defined(_WIN32)
#define YICAD_PLUGIN_CALL __cdecl
#define YICAD_PLUGIN_EXPORT_ATTRIBUTE __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define YICAD_PLUGIN_CALL
#define YICAD_PLUGIN_EXPORT_ATTRIBUTE __attribute__((visibility("default")))
#else
#define YICAD_PLUGIN_CALL
#define YICAD_PLUGIN_EXPORT_ATTRIBUTE
#endif

#if defined(__cplusplus)
#define YICAD_PLUGIN_EXTERN_C extern "C"
#else
#define YICAD_PLUGIN_EXTERN_C extern
#endif

#if defined(YICAD_PLUGIN_BUILD)
#define YICAD_PLUGIN_API \
    YICAD_PLUGIN_EXTERN_C YICAD_PLUGIN_EXPORT_ATTRIBUTE
#else
#define YICAD_PLUGIN_API YICAD_PLUGIN_EXTERN_C
#endif

#define YICAD_PLUGIN_EXPORT YICAD_PLUGIN_API

typedef int32_t YiCadResult;

#define YICAD_FAILURE ((YiCadResult)0)
#define YICAD_SUCCESS ((YiCadResult)1)

typedef void* YiCadDocumentHandle;

/* All strings crossing the ABI are UTF-8. Document handles are non-owning. */

typedef void (YICAD_PLUGIN_CALL *YiCadCommandCallback)(void* userData);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadImportCallback)(
    YiCadDocumentHandle document,
    const char* filePath,
    void* userData);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadExportCallback)(
    YiCadDocumentHandle document,
    const char* filePath,
    void* userData);

typedef void (YICAD_PLUGIN_CALL *YiCadMessageFn)(const char* text);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadRegisterCommandFn)(
    const char* pluginId,
    const char* commandId,
    const char* displayName,
    YiCadCommandCallback callback,
    void* userData);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadRegisterRibbonButtonFn)(
    const char* pluginId,
    const char* tab,
    const char* group,
    const char* commandId,
    const char* iconPath);
typedef YiCadDocumentHandle (YICAD_PLUGIN_CALL *YiCadCurrentDocumentFn)(void);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadDocumentAddLineFn)(
    YiCadDocumentHandle document,
    double x1,
    double y1,
    double x2,
    double y2);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadDocumentAddCircleFn)(
    YiCadDocumentHandle document,
    double centerX,
    double centerY,
    double radius);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadDocumentRegenFn)(
    YiCadDocumentHandle document);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadDocumentZoomAutoFn)(
    YiCadDocumentHandle document);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadRegisterImportFilterFn)(
    const char* pluginId,
    const char* formatId,
    const char* displayName,
    const char* extension,
    YiCadImportCallback callback,
    void* userData);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadRegisterExportFilterFn)(
    const char* pluginId,
    const char* formatId,
    const char* displayName,
    const char* extension,
    YiCadExportCallback callback,
    void* userData);

typedef struct YiCadHostApi
{
    uint32_t structSize;
    uint32_t abiVersion;
    YiCadMessageFn message;
    YiCadRegisterCommandFn registerCommand;
    YiCadRegisterRibbonButtonFn registerRibbonButton;
    YiCadCurrentDocumentFn currentDocument;
    YiCadDocumentAddLineFn documentAddLine;
    YiCadDocumentAddCircleFn documentAddCircle;
    YiCadDocumentRegenFn documentRegen;
    YiCadDocumentZoomAutoFn documentZoomAuto;
    YiCadRegisterImportFilterFn registerImportFilter;
    YiCadRegisterExportFilterFn registerExportFilter;
} YiCadHostApi;

typedef struct YiCadPluginApi
{
    uint32_t structSize;
    uint32_t abiVersion;
    const char* pluginId;
    const char* pluginName;
    const char* pluginVersion;
} YiCadPluginApi;

/** @brief ABI v1 宿主函数表前缀的字节数。 */
#define YICAD_HOST_API_V1_SIZE                                            \
    ((uint32_t)(offsetof(YiCadHostApi, registerExportFilter) +           \
                sizeof(((YiCadHostApi*)0)->registerExportFilter)))
/** @brief ABI v1 插件输出表前缀的字节数。 */
#define YICAD_PLUGIN_API_V1_SIZE                                          \
    ((uint32_t)(offsetof(YiCadPluginApi, pluginVersion) +                 \
                sizeof(((YiCadPluginApi*)0)->pluginVersion)))

typedef uint32_t (YICAD_PLUGIN_CALL *YiCadPluginGetAbiVersionFn)(void);
typedef YiCadResult (YICAD_PLUGIN_CALL *YiCadPluginInitFn)(
    const YiCadHostApi* host,
    YiCadPluginApi* plugin);
typedef void (YICAD_PLUGIN_CALL *YiCadPluginShutdownFn)(void);

/** @brief 返回插件实现的最高 C ABI 版本。 */
YICAD_PLUGIN_API uint32_t YICAD_PLUGIN_CALL
yicad_plugin_get_abi_version(void);
YICAD_PLUGIN_API YiCadResult YICAD_PLUGIN_CALL
yicad_plugin_init(const YiCadHostApi* host, YiCadPluginApi* plugin);
YICAD_PLUGIN_API void YICAD_PLUGIN_CALL
yicad_plugin_shutdown(void);

#if defined(__cplusplus)
#define YICAD_ABI_STATIC_ASSERT(condition, message) static_assert(condition, message)
#define YICAD_ABI_ALIGNOF(type) alignof(type)
#else
#define YICAD_ABI_STATIC_ASSERT(condition, message) _Static_assert(condition, message)
#define YICAD_ABI_ALIGNOF(type) _Alignof(type)
#endif

#define YICAD_ABI_FIELD_FOLLOWS(type, field, previousField)                  \
    YICAD_ABI_STATIC_ASSERT(                                                 \
        offsetof(type, field) >=                                            \
            offsetof(type, previousField) +                                 \
                sizeof(((type*)0)->previousField),                          \
        #type "." #field " must follow " #previousField)

#define YICAD_ABI_FIELD_AT(type, field, expectedOffset)                      \
    YICAD_ABI_STATIC_ASSERT(                                                 \
        offsetof(type, field) == (expectedOffset),                          \
        #type "." #field " offset snapshot changed")

YICAD_ABI_STATIC_ASSERT(sizeof(uint32_t) == 4, "uint32_t must be 32-bit");
YICAD_ABI_STATIC_ASSERT(sizeof(YiCadResult) == 4, "YiCadResult must be 32-bit");
YICAD_ABI_STATIC_ASSERT(YICAD_FAILURE == 0, "failure must be zero");
YICAD_ABI_STATIC_ASSERT(YICAD_SUCCESS == 1, "success must be one");
YICAD_ABI_STATIC_ASSERT(
    YICAD_PLUGIN_ABI_V1 == UINT32_C(1),
    "ABI v1 version snapshot changed");
YICAD_ABI_STATIC_ASSERT(
    YICAD_PLUGIN_ABI_MIN_VERSION <= YICAD_PLUGIN_ABI_MAX_VERSION,
    "invalid supported ABI version range");

YICAD_ABI_STATIC_ASSERT(
    offsetof(YiCadHostApi, structSize) == 0,
    "YiCadHostApi.structSize must be first");
YICAD_ABI_STATIC_ASSERT(
    offsetof(YiCadHostApi, abiVersion) == sizeof(uint32_t),
    "YiCadHostApi.abiVersion must be second");
YICAD_ABI_FIELD_FOLLOWS(YiCadHostApi, message, abiVersion);
YICAD_ABI_FIELD_FOLLOWS(YiCadHostApi, registerCommand, message);
YICAD_ABI_FIELD_FOLLOWS(YiCadHostApi, registerRibbonButton, registerCommand);
YICAD_ABI_FIELD_FOLLOWS(YiCadHostApi, currentDocument, registerRibbonButton);
YICAD_ABI_FIELD_FOLLOWS(YiCadHostApi, documentAddLine, currentDocument);
YICAD_ABI_FIELD_FOLLOWS(YiCadHostApi, documentAddCircle, documentAddLine);
YICAD_ABI_FIELD_FOLLOWS(YiCadHostApi, documentRegen, documentAddCircle);
YICAD_ABI_FIELD_FOLLOWS(YiCadHostApi, documentZoomAuto, documentRegen);
YICAD_ABI_FIELD_FOLLOWS(YiCadHostApi, registerImportFilter, documentZoomAuto);
YICAD_ABI_FIELD_FOLLOWS(
    YiCadHostApi,
    registerExportFilter,
    registerImportFilter);
YICAD_ABI_STATIC_ASSERT(
    sizeof(YiCadHostApi) >=
        offsetof(YiCadHostApi, registerExportFilter) +
            sizeof(((YiCadHostApi*)0)->registerExportFilter),
    "YiCadHostApi must contain its final field");

YICAD_ABI_STATIC_ASSERT(
    offsetof(YiCadPluginApi, structSize) == 0,
    "YiCadPluginApi.structSize must be first");
YICAD_ABI_STATIC_ASSERT(
    offsetof(YiCadPluginApi, abiVersion) == sizeof(uint32_t),
    "YiCadPluginApi.abiVersion must be second");
YICAD_ABI_FIELD_FOLLOWS(YiCadPluginApi, pluginId, abiVersion);
YICAD_ABI_FIELD_FOLLOWS(YiCadPluginApi, pluginName, pluginId);
YICAD_ABI_FIELD_FOLLOWS(YiCadPluginApi, pluginVersion, pluginName);
YICAD_ABI_STATIC_ASSERT(
    sizeof(YiCadPluginApi) >=
        offsetof(YiCadPluginApi, pluginVersion) +
            sizeof(((YiCadPluginApi*)0)->pluginVersion),
    "YiCadPluginApi must contain its final field");

#if defined(_WIN32) && UINTPTR_MAX == UINT64_MAX
YICAD_ABI_STATIC_ASSERT(
    sizeof(YiCadHostApi) >= YICAD_HOST_API_V1_SIZE,
    "Win64 host ABI lost its v1 prefix");
YICAD_ABI_STATIC_ASSERT(
    YICAD_HOST_API_V1_SIZE == 88,
    "unexpected Win64 host ABI v1 size");
YICAD_ABI_STATIC_ASSERT(
    YICAD_ABI_ALIGNOF(YiCadHostApi) == 8,
    "unexpected Win64 host ABI alignment");
YICAD_ABI_FIELD_AT(YiCadHostApi, message, 8);
YICAD_ABI_FIELD_AT(YiCadHostApi, registerCommand, 16);
YICAD_ABI_FIELD_AT(YiCadHostApi, registerRibbonButton, 24);
YICAD_ABI_FIELD_AT(YiCadHostApi, currentDocument, 32);
YICAD_ABI_FIELD_AT(YiCadHostApi, documentAddLine, 40);
YICAD_ABI_FIELD_AT(YiCadHostApi, documentAddCircle, 48);
YICAD_ABI_FIELD_AT(YiCadHostApi, documentRegen, 56);
YICAD_ABI_FIELD_AT(YiCadHostApi, documentZoomAuto, 64);
YICAD_ABI_FIELD_AT(YiCadHostApi, registerImportFilter, 72);
YICAD_ABI_FIELD_AT(YiCadHostApi, registerExportFilter, 80);
YICAD_ABI_STATIC_ASSERT(
    sizeof(YiCadPluginApi) >= YICAD_PLUGIN_API_V1_SIZE,
    "Win64 plugin ABI lost its v1 prefix");
YICAD_ABI_STATIC_ASSERT(
    YICAD_PLUGIN_API_V1_SIZE == 32,
    "unexpected Win64 plugin ABI v1 size");
YICAD_ABI_STATIC_ASSERT(
    YICAD_ABI_ALIGNOF(YiCadPluginApi) == 8,
    "unexpected Win64 plugin ABI alignment");
YICAD_ABI_FIELD_AT(YiCadPluginApi, pluginId, 8);
YICAD_ABI_FIELD_AT(YiCadPluginApi, pluginName, 16);
YICAD_ABI_FIELD_AT(YiCadPluginApi, pluginVersion, 24);
#elif defined(_WIN32) && UINTPTR_MAX == UINT32_MAX
YICAD_ABI_STATIC_ASSERT(
    sizeof(YiCadHostApi) >= YICAD_HOST_API_V1_SIZE,
    "Win32 host ABI lost its v1 prefix");
YICAD_ABI_STATIC_ASSERT(
    YICAD_HOST_API_V1_SIZE == 48,
    "unexpected Win32 host ABI v1 size");
YICAD_ABI_STATIC_ASSERT(
    YICAD_ABI_ALIGNOF(YiCadHostApi) == 4,
    "unexpected Win32 host ABI alignment");
YICAD_ABI_FIELD_AT(YiCadHostApi, message, 8);
YICAD_ABI_FIELD_AT(YiCadHostApi, registerCommand, 12);
YICAD_ABI_FIELD_AT(YiCadHostApi, registerRibbonButton, 16);
YICAD_ABI_FIELD_AT(YiCadHostApi, currentDocument, 20);
YICAD_ABI_FIELD_AT(YiCadHostApi, documentAddLine, 24);
YICAD_ABI_FIELD_AT(YiCadHostApi, documentAddCircle, 28);
YICAD_ABI_FIELD_AT(YiCadHostApi, documentRegen, 32);
YICAD_ABI_FIELD_AT(YiCadHostApi, documentZoomAuto, 36);
YICAD_ABI_FIELD_AT(YiCadHostApi, registerImportFilter, 40);
YICAD_ABI_FIELD_AT(YiCadHostApi, registerExportFilter, 44);
YICAD_ABI_STATIC_ASSERT(
    sizeof(YiCadPluginApi) >= YICAD_PLUGIN_API_V1_SIZE,
    "Win32 plugin ABI lost its v1 prefix");
YICAD_ABI_STATIC_ASSERT(
    YICAD_PLUGIN_API_V1_SIZE == 20,
    "unexpected Win32 plugin ABI v1 size");
YICAD_ABI_STATIC_ASSERT(
    YICAD_ABI_ALIGNOF(YiCadPluginApi) == 4,
    "unexpected Win32 plugin ABI alignment");
YICAD_ABI_FIELD_AT(YiCadPluginApi, pluginId, 8);
YICAD_ABI_FIELD_AT(YiCadPluginApi, pluginName, 12);
YICAD_ABI_FIELD_AT(YiCadPluginApi, pluginVersion, 16);
#endif

#if defined(__cplusplus)
namespace yicad_plugin_abi_detail
{

template<typename Left, typename Right>
struct IsSame
{
    enum
    {
        value = 0
    };
};

template<typename Type>
struct IsSame<Type, Type>
{
    enum
    {
        value = 1
    };
};

} // namespace yicad_plugin_abi_detail

YICAD_ABI_STATIC_ASSERT(
    __is_standard_layout(YiCadHostApi),
    "YiCadHostApi must have standard layout");
YICAD_ABI_STATIC_ASSERT(
    __is_standard_layout(YiCadPluginApi),
    "YiCadPluginApi must have standard layout");
YICAD_ABI_STATIC_ASSERT(
    yicad_plugin_abi_detail::IsSame<
        decltype(&yicad_plugin_get_abi_version),
        YiCadPluginGetAbiVersionFn>::value,
    "ABI version entry point signature changed");
YICAD_ABI_STATIC_ASSERT(
    yicad_plugin_abi_detail::IsSame<
        decltype(&yicad_plugin_init),
        YiCadPluginInitFn>::value,
    "plugin init entry point signature changed");
YICAD_ABI_STATIC_ASSERT(
    yicad_plugin_abi_detail::IsSame<
        decltype(&yicad_plugin_shutdown),
        YiCadPluginShutdownFn>::value,
    "plugin shutdown entry point signature changed");
#else
YICAD_ABI_STATIC_ASSERT(
    _Generic(
        &yicad_plugin_get_abi_version,
        YiCadPluginGetAbiVersionFn: 1,
        default: 0),
    "ABI version entry point signature changed");
YICAD_ABI_STATIC_ASSERT(
    _Generic(&yicad_plugin_init, YiCadPluginInitFn: 1, default: 0),
    "plugin init entry point signature changed");
YICAD_ABI_STATIC_ASSERT(
    _Generic(&yicad_plugin_shutdown, YiCadPluginShutdownFn: 1, default: 0),
    "plugin shutdown entry point signature changed");
#endif

#undef YICAD_ABI_FIELD_FOLLOWS
#undef YICAD_ABI_FIELD_AT
#undef YICAD_ABI_ALIGNOF
#undef YICAD_ABI_STATIC_ASSERT

#endif
