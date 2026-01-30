# xmp_toolkit_sdk

Original upstream repository: https://github.com/adobe/xmp-toolkit-sdk

This repository contains the third-party open-source **XMP Toolkit SDK** (Adobe XMP Toolkit SDK), which is used to parse, read, write, and serialize **XMP (Extensible Metadata Platform)** metadata.
In OpenHarmony, **xmp_toolkit_sdk** is mainly used as a foundational component of the media subsystem, providing XMP metadata read/write capability for media files such as images.

## Directory Structure

```
build/              Build helper scripts
docs/               Documentation (including API docs, technical notes, etc.)
public/             Exported public headers and wrappers (for upper-layer components)
samples/            Official sample code
source/             Toolkit adaptations and platform-related implementations (e.g., IO adapter, Host/Expat adapter, etc.)
third-party/        Third-party dependencies (e.g., expat, zlib, etc.)
tools/              Helper tools and scripts
XMPCommon/          Common XMP foundation module (common interfaces, error codes, utilities, etc.)
XMPCore/            Core XMP DOM / metadata model implementation
XMPFiles/           File-format-oriented XMP read/write implementation (handles XMP packets by file type)
XMPFilesPlugins/    XMPFiles plugin-related content (enable/build as needed)
README.md           README
```

## How to Use xmp_toolkit_sdk in OpenHarmony

OpenHarmony system components need to reference the **xmpsdk** component in `BUILD.gn` to use it:

```
// BUILD.gn
external_deps += [ "xmp_toolkit_sdk:xmpsdk" ]
```

## Read/Write XMP Metadata with xmp_toolkit_sdk

The following example mainly uses the native C++ APIs of the XMP Toolkit SDK, demonstrating a typical workflow:
**open a file → read XMP → modify properties → write back to the file**.

### (1) Initialize the global state of XMP Toolkit

```
 // XMPMeta
 static bool SXMPMeta::Initialize();
 static void SXMPMeta::Terminate();

 // XMPFiles
 static bool SXMPFiles::Initialize(XMP_OptionBits options = 0);
 static void SXMPFiles::Terminate();
```

### (2) Open a file and read XMP

```
bool SXMPFiles::OpenFile(XMP_StringPtr filePath,
                         XMP_FileFormat format = kXMP_UnknownFile,
                         XMP_OptionBits openFlags = 0);

bool SXMPFiles::GetXMP(SXMPMeta *xmpObj = 0,
                       tStringObj *xmpPacket = 0,
                       XMP_PacketInfo *packetInfo = 0);
```

Example:

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

### (3) Get/Set/Delete XMP properties

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

Example:

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

### (4) Serialize / parse XMP (RDF/XML)

```
void SXMPMeta::ParseFromBuffer(XMP_StringPtr buffer,
                               XMP_StringLen bufferSize,
                               XMP_OptionBits options = 0);

void SXMPMeta::SerializeToBuffer(tStringObj *rdfString,
                                 XMP_OptionBits options = 0,
                                 XMP_StringLen padding = 0) const;
```

Example:

```cpp
std::string rdf;
xmp.SerializeToBuffer(&rdf, kXMP_OmitPacketWrapper);

SXMPMeta parsed;
parsed.ParseFromBuffer(rdf.c_str(), static_cast<XMP_StringLen>(rdf.size()));
```

### (5) Write back to the file and close

```
bool SXMPFiles::CanPutXMP(const SXMPMeta &xmpObj);
void SXMPFiles::PutXMP(const SXMPMeta &xmpObj);
void SXMPFiles::CloseFile(XMP_OptionBits closeFlags = 0);
```

Example:

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

## Feature Support Notes

The xmp_toolkit_sdk integrated into OpenHarmony provides XMP metadata read/write capability, mainly targeting common image formats such as **JPEG, PNG, and GIF** (the exact supported scope depends on the actual integration and build configuration).

## License

xmp_toolkit_sdk uses the open-source license corresponding to Adobe XMP Toolkit SDK. See the `LICENSE` file in the repository root directory for details.
