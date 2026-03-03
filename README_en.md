 # xmp_toolkit_sdk
 
 Upstream repository: https://github.com/adobe/xmp-toolkit-sdk
 
 This repository contains the third-party open-source software XMP Toolkit SDK (Adobe XMP Toolkit SDK). It is used to parse, read, write, and serialize XMP (Extensible Metadata Platform) metadata.
 
 In OpenHarmony, `xmp_toolkit_sdk` is mainly used as a foundational component of the multimedia subsystem, providing XMP metadata read/write capabilities for media files such as images.
 
 ## OpenHarmony Integration Architecture
 
 ![](figures/en-us_XMPSDK_intergrated_arch.png)
 
 ## Directory Structure
 
 ```
 build/              Build helper scripts
 docs/               Documentation (including API docs, technical notes, etc.)
 public/             Public headers and related wrappers (for upper-layer components)
 samples/            Official sample code
 source/             Toolkit adaptation and platform-specific implementations (e.g., IO adapter, Host/Expat adapter)
 third-party/        Third-party dependencies (expat, zlib)
 tools/              Auxiliary tools and scripts
 XMPCommon/          Common base module (generic interfaces, error codes, utilities, etc.)
 XMPCore/            Core DOM/metadata model implementation
 XMPFiles/           XMP read/write implementation for file formats (handles XMP packets by file type)
 XMPFilesPlugins/    XMPFiles plugins (enable/build as needed)
 README.md           README
 ```
 
 ## How Developers Use It
 
 ### Device Developers
 
 #### How to Introduce `xmp_toolkit_sdk` into System Components
 
 This repo is located at `//third_party/xmp_toolkit_sdk` and is used for system components to link XMP Toolkit SDK at build time.
 
 Steps to add the dependency for a system component:
 
 1. Add an external dependency in the consumer's `bundle.json`:
 
    Using Image Framework as an example, the file path is `//foundation/multimedia/image_framework/bundle.json`.
 
    ```
     "deps": {
            "components": [
              "xmp_toolkit_sdk"
            ]
     }
    ```
 
 2. Add the dependency in the consumer's `BUILD.gn`:
 
    Using Image Framework as an example, add it in `//foundation/multimedia/image_framework/interfaces/innerkits/BUILD.gn`.
 
    ```
    external_deps += [ "xmp_toolkit_sdk:xmpsdk" ]
    ```
 
 Note: The build artifact of `xmp_toolkit_sdk` is `libxmpsdk.so`, which is a **dynamic library dependency**. It is typically loaded automatically by the dynamic linker when the process starts, and the business side does not need to explicitly call `dlopen`.
 
 #### Read/Write XMP Metadata Using `xmp_toolkit_sdk`
 
 The following examples are primarily based on the native C++ APIs of XMP Toolkit SDK and demonstrate a typical workflow of “open file, read XMP, modify properties, and write back to the file”.
 
 (1) Initialize the global state of XMP Toolkit.
 
 ```
 // XMPMeta
 static bool SXMPMeta::Initialize();
 static void SXMPMeta::Terminate();
 
 // XMPFiles
 static bool SXMPFiles::Initialize(XMP_OptionBits options = 0);
 static void SXMPFiles::Terminate();
 ```
 
 (2) Open a file and read XMP.
 
 ```
 bool SXMPFiles::OpenFile(XMP_StringPtr filePath,
                          XMP_FileFormat format = kXMP_UnknownFile,
                          XMP_OptionBits openFlags = 0);
 
 bool SXMPFiles::GetXMP(SXMPMeta *xmpObj = 0,
                        tStringObj *xmpPacket = 0,
                        XMP_PacketInfo *packetInfo = 0);
 ```
 
 A concrete example:
 
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
 
 (3) Get/Set/Delete XMP properties.
 
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
 
 A concrete example:
 
 ```cpp
 std::string value;
 XMP_OptionBits options = kXMP_NoOptions;
 
 // Use Dublin Core as an example
 const char *schemaNS = kXMP_NS_DC;
 const char *propName = "title";
 
 xmp.SetProperty(schemaNS, propName, "example_title", kXMP_NoOptions);
 if (xmp.GetProperty(schemaNS, propName, &value, &options)) {
     // value: "example_title"
 }
 xmp.DeleteProperty(schemaNS, propName);
 ```
 
 (4) Serialize/Deserialize XMP (RDF/XML).
 
 ```
 void SXMPMeta::ParseFromBuffer(XMP_StringPtr buffer,
                                XMP_StringLen bufferSize,
                                XMP_OptionBits options = 0);
 
 void SXMPMeta::SerializeToBuffer(tStringObj *rdfString,
                                  XMP_OptionBits options = 0,
                                  XMP_StringLen padding = 0) const;
 ```
 
 A concrete example:
 
 ```cpp
 std::string rdf;
 xmp.SerializeToBuffer(&rdf, kXMP_OmitPacketWrapper);
 
 SXMPMeta parsed;
 parsed.ParseFromBuffer(rdf.c_str(), static_cast<XMP_StringLen>(rdf.size()));
 ```
 
 (5) Write back to the file and close.
 
 ```
 bool SXMPFiles::CanPutXMP(const SXMPMeta &xmpObj);
 void SXMPFiles::PutXMP(const SXMPMeta &xmpObj);
 void SXMPFiles::CloseFile(XMP_OptionBits closeFlags = 0);
 ```
 
 A concrete example:
 
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
 
 ### Application Developers
 
 The C++ APIs of XMP Toolkit SDK are not directly exposed to third-party applications.
 
 For application developers, some image-related XMP capabilities are provided by the Image Framework subsystem as public APIs. Typical capabilities include:
 
 - **Read image XMP metadata**: read an XMP metadata object from an image source
 - **Write image XMP metadata**: write XMP metadata back to an image source
 - **Read/Set/Delete XMP tags by path**: such as `xmp:title`
 - **Enumerate/batch-get XMP tags**: traverse tags under a specified root path
 - **Read/Write XMP in binary form**: get a blob or overwrite using a blob
 - **Register namespaces and prefixes**: register a namespace before operating on custom tags
 
 Typical API examples (APIs provided by Image Framework for application developers; snippets are simplified):
 
 **1) Read image XMP metadata**
 
 API prototype: `readXMPMetadata(): Promise<XMPMetadata | null>;`
 
 Example:
 
 ```
 import { image } from '@kit.ImageKit';
 
 const imageSource: image.ImageSource = image.createImageSource(filePath);
 const xmpMetadata: image.XMPMetadata | null = await imageSource.readXMPMetadata();
 ```
 
 **2) Write image XMP metadata**
 
 API prototype: `writeXMPMetadata(xmpMetadata: XMPMetadata): Promise<void>;`
 
 Example:
 
 ```
 import { image } from '@kit.ImageKit';
 
 const imageSource: image.ImageSource = image.createImageSource(filePath);
 let xmpMetadata = new image.XMPMetadata();
 await xmpMetadata.setValue('xmp:title', image.XMPTagType.SIMPLE, 'My Title');
 await imageSource.writeXMPMetadata(xmpMetadata);
 ```
 
 **3) Register a namespace prefix**
 
 API prototype: `registerNamespacePrefix(xmlns: string, prefix: string): Promise<void>;`
 
 Example:
 
 ```
 import { image } from '@kit.ImageKit';
 
 const xmpMetadata = new image.XMPMetadata();
 await xmpMetadata.registerNamespacePrefix('http://mybook.com/story/1.0/', 'book');
 ```
 
 **4) Set a tag by path**
 
 API prototype: `setValue(path: string, type: XMPTagType, value?: string): Promise<void>;`
 
 Example:
 
 ```
 import { image } from '@kit.ImageKit';
 
 await xmpMetadata.setValue('xmp:title', image.XMPTagType.SIMPLE, 'My Title');
 ```
 
 **5) Read a tag by path**
 
 API prototype: `getTag(path: string): Promise<XMPTag | null>;`
 
 Example:
 
 ```
 import { image } from '@kit.ImageKit';
 
 const tag: image.XMPTag | null = await xmpMetadata.getTag('xmp:title');
 ```
 
 **6) Enumerate tags**
 
 API prototype: `enumerateTags(callback: (path: string, tag: XMPTag) => boolean, rootPath?: string, options?: XMPEnumerateOption): void;`
 
 Example:
 
 ```
 import { image } from '@kit.ImageKit';
 
 xmpMetadata.enumerateTags((path: string, tag: image.XMPTag): boolean => {
   console.info(`path=${path}, value=${tag.value}`);
   return false;
 }, undefined, { isRecursive: true });
 ```
 
 **7) Read/Write XMP in binary form**
 
 API prototypes:
 
 + Binary read: `getBlob(): Promise<ArrayBuffer>;`
 + Binary write: `setBlob(buffer: ArrayBuffer): Promise<void>;`
 
 Example:
 
 ```
 const blob: ArrayBuffer = await xmpMetadata.getBlob();
 await xmpMetadata.setBlob(blob);
 ```
 
 For more API definitions, supported formats, detailed specifications, and error codes, refer to the Image Framework documentation.
 
 ## Supported Features
 
 The `xmp_toolkit_sdk` integrated in OpenHarmony provides XMP metadata read/write capabilities, mainly for common image formats such as JPEG, PNG, and GIF (the actual supported scope depends on the integration and build configuration).
 
 ## Other Third-Party Dependencies
 
 The main external dependencies of `xmp_toolkit_sdk` in OpenHarmony are as follows:
 
 - **expat**
   - **Purpose**: XML parsing.
   - **Where used**: XML/RDF parsing adaptation layer of XMPCore (e.g., `XMPCore/source/ExpatAdapter.cpp` includes `expat.h` and parses XMP RDF/XML via APIs such as `XML_ParserCreateNS` and `XML_Parse`).
 
 - **zlib**
   - **Purpose**: compression/decompression (deflate/inflate).
   - **Where used**: used by XMPFiles when handling some data wrapped in compressed form (e.g., `XMPFiles/source/FormatSupport/SWF_Support.cpp`, `XMPFiles/source/FileHandlers/UCF_Handler.cpp`, etc. include `zlib.h` and call `inflate/deflate`).
 
 ## License
 
 `xmp_toolkit_sdk` uses the open-source license of Adobe XMP Toolkit SDK. See the `LICENSE` file in the repository root directory.
