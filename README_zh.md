# xmp_toolkit_sdk

原始仓库来源：https://github.com/adobe/xmp-toolkit-sdk


仓库包含第三方开源软件 XMP Toolkit SDK（Adobe XMP Toolkit SDK），用于解析、读取、写入和序列化 XMP（Extensible Metadata Platform）元数据。
在 OpenHarmony 中，xmp_toolkit_sdk 主要作为媒体子系统的基础组件，为图片等媒体文件提供 XMP 元数据的读写能力。

## 目录结构

```
build/              构建辅助脚本
docs/               说明文档（包含 API 文档、技术说明等）
public/             对外发布头文件及相关封装（供上层组件引用）
samples/            官方示例代码
source/             工具包适配与平台相关实现（如 IO 适配、Host/Expat 适配等）
third-party/        第三方依赖（如 expat、zlib 等）
tools/              辅助工具与脚本
XMPCommon/          XMP 公共基础模块（通用接口、错误码、工具类等）
XMPCore/            XMP 核心 DOM/元数据模型实现
XMPFiles/           面向文件格式的 XMP 读写实现（按文件类型处理 XMP 包）
XMPFilesPlugins/    XMPFiles 插件相关内容（按需启用/构建）
README.md           README 说明
```

## OpenHarmony如何使用xmp_toolkit_sdk

OpenHarmony的系统部件需要在BUILD.gn中引用xmpsdk部件以使用xmpsdk。
```
// BUILD.gn
external_deps += [ "xmp_toolkit_sdk:xmpsdk" ]
```

## 使用xmp_toolkit_sdk读写xmp元数据

以下示例以 XMP Toolkit SDK 的原生 C++ 接口为主，展示典型的“打开文件、读取 XMP、修改属性、写回文件”的流程。

(1) 初始化 XMP Toolkit 的全局状态。

  ```
  // XMPMeta
  static bool SXMPMeta::Initialize();
  static void SXMPMeta::Terminate();

  // XMPFiles
  static bool SXMPFiles::Initialize(XMP_OptionBits options = 0);
  static void SXMPFiles::Terminate();
  ```

(2) 打开文件并读取 XMP。

  ```
  bool SXMPFiles::OpenFile(XMP_StringPtr filePath,
                           XMP_FileFormat format = kXMP_UnknownFile,
                           XMP_OptionBits openFlags = 0);

  bool SXMPFiles::GetXMP(SXMPMeta *xmpObj = 0,
                         tStringObj *xmpPacket = 0,
                         XMP_PacketInfo *packetInfo = 0);
  ```

  具体示例如：
  ```cpp
  #include "XMP.hpp"
  #include "XMP.incl_cpp"

  SXMPMeta xmp;
  SXMPFiles xmpFiles;

  SXMPMeta::Initialize();
  SXMPFiles::Initialize(kXMPFiles_IgnoreLocalText);

  const char *filePath = "/data/test.jpg";
  const XMP_OptionBits openFlags = kXMPFiles_OpenForRead;
  if (xmpFiles.OpenFile(filePath, kXMP_UnknownFile, openFlags)) {
      if (xmpFiles.GetXMP(&xmp)) {
          // Read success
      }
      xmpFiles.CloseFile();
  }

  SXMPFiles::Terminate();
  SXMPMeta::Terminate();
  ```

(3) 读取/设置/删除 XMP 属性。

  ```
  void SXMPMeta::SetProperty(XMP_StringPtr schemaNS,
                             XMP_StringPtr propName,
                             XMP_StringPtr propValue,
                             XMP_OptionBits options = 0);

  bool SXMPMeta::GetProperty(XMP_StringPtr schemaNS,
                             XMP_StringPtr propName,
                             tStringObj *propValue,
                             XMP_OptionBits *options) const;

  void SXMPMeta::DeleteProperty(XMP_StringPtr schemaNS,
                                XMP_StringPtr propName);
  ```

  具体示例如：
  ```cpp
  std::string value;
  XMP_OptionBits options = kXMP_NoOptions;

  // 以 Dublin Core 为例
  const char *schemaNS = kXMP_NS_DC;
  const char *propName = "title";

  xmp.SetProperty(schemaNS, propName, "example_title", kXMP_NoOptions);
  if (xmp.GetProperty(schemaNS, propName, &value, &options)) {
      // value: "example_title"
  }
  xmp.DeleteProperty(schemaNS, propName);
  ```

(4) 序列化/反序列化 XMP（RDF/XML）。

  ```
  void SXMPMeta::ParseFromBuffer(XMP_StringPtr buffer,
                                 XMP_StringLen bufferSize,
                                 XMP_OptionBits options = 0);

  void SXMPMeta::SerializeToBuffer(tStringObj *rdfString,
                                   XMP_OptionBits options = 0,
                                   XMP_StringLen padding = 0) const;
  ```

  具体示例如：
  ```cpp
  std::string rdf;
  xmp.SerializeToBuffer(&rdf, kXMP_OmitPacketWrapper);

  SXMPMeta parsed;
  parsed.ParseFromBuffer(rdf.c_str(), static_cast<XMP_StringLen>(rdf.size()));
  ```

(5) 写回文件并关闭。

  ```
  bool SXMPFiles::CanPutXMP(const SXMPMeta &xmpObj);
  void SXMPFiles::PutXMP(const SXMPMeta &xmpObj);
  void SXMPFiles::CloseFile(XMP_OptionBits closeFlags = 0);
  ```

  具体示例如：
  ```cpp
  const XMP_OptionBits updateFlags = kXMPFiles_OpenForUpdate;
  if (xmpFiles.OpenFile(filePath, kXMP_UnknownFile, updateFlags)) {
      // GetXMP -> Modify xmp -> PutXMP
      if (xmpFiles.GetXMP(&xmp)) {
          xmp.SetProperty(kXMP_NS_XMP, "CreatorTool", "OpenHarmony", kXMP_NoOptions);
          if (xmpFiles.CanPutXMP(xmp)) {
              xmpFiles.PutXMP(xmp);
          }
      }
      xmpFiles.CloseFile();
  }
  ```

## 功能支持说明

OpenHarmony 集成的 xmp_toolkit_sdk 用于提供 XMP 元数据的读写能力，主要面向 JPEG、PNG、GIF 等常见图片格式（具体支持范围以实际接入与编译配置为准）。

## License

xmp_toolkit_sdk 使用 Adobe XMP Toolkit SDK 对应的开源协议，详见仓库根目录下的 `LICENSE` 文件。
